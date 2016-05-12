#include <math.h>
#include <thread>
#include "Random.h"
#include "UCTSample.h"
#include "Debug.h"

const float C = 1.0; // UCB定数
const int FPU = 10; // First Play Urgency
const int THR = 15; // ノード展開の閾値

int PLAYOUT_MAX = 2000;

const int NODE_MAX = 300000;
UCTNode node_pool[NODE_MAX]; // ノードプール(高速化のため動的に確保しない)
volatile long node_pool_cnt = 0;

thread_local Random random;

UCTNode* create_root_node()
{
	node_pool_cnt = 1; // rootノード作成時はロック不要
	node_pool[0].playout_num_sum = 0;
	node_pool[0].child_num = 0;
	return node_pool;
}

UCTNode* create_child_node(const int size)
{
	// アトミックに加算
	long prev_note_pool_cnt = _InterlockedExchangeAdd(&node_pool_cnt, size);

	if (node_pool_cnt > NODE_MAX)
	{
		return nullptr;
	}

	return node_pool + prev_note_pool_cnt;
}

// ノード展開
bool UCTNode::expand_node(const Board& board)
{
	// 空白の数をカウント
	child_num = BOARD_STONE_MAX - (board.stone_num[BLACK] + board.stone_num[WHITE]) + 1;

	// ノードを確保
	child = create_child_node(child_num);

	if (child == nullptr)
	{
		child_num = 0;
		return false;
	}

	// ノードの値を設定
	int i = 0;
	for (XY y = BOARD_WIDTH; y < BOARD_MAX - BOARD_WIDTH; y += BOARD_WIDTH)
	{
		for (XY x = 1; x <= BOARD_SIZE; x++)
		{
			XY xy = y + x;
			if (board.is_empty(xy))
			{
				child[i].xy = xy;
				child[i].playout_num = 0;
				child[i].playout_num_sum = 0;
				child[i].win_num = 0;
				child[i].child_num = 0;
				i++;
			}
		}
	}
	// PASSを追加
	child[i].xy = PASS;
	child[i].playout_num = 0;
	child[i].playout_num_sum = 0;
	child[i].win_num = 0;
	child[i].child_num = 0;

	return true;
}

// 終局 勝敗を返す
Color UCTSample::end_game(const Board& board)
{
	// 中国ルールで数える
	int score = board.stone_num[BLACK] - board.stone_num[WHITE];
	int eye_num = BOARD_STONE_MAX - (board.stone_num[BLACK] + board.stone_num[WHITE]);
	double final_score = score - KOMI;

	// 眼を数えなくても勝敗が決まる場合
	if (final_score - eye_num > 0)
	{
		// 眼が全て白だとしても黒の勝ち
		return BLACK;
	}
	else if (final_score + eye_num < 0)
	{
		// 眼が全て黒だとしても白の勝ち
		return WHITE;
	}

	//debug_print_board(board);
	//printf("black = %d, white = %d, score = %d\n", board.stone_num[BLACK], board.stone_num[WHITE], score);

	// 眼を数える
	for (XY y = BOARD_WIDTH; y < BOARD_MAX - BOARD_WIDTH; y += BOARD_WIDTH)
	{
		for (XY x = 1; x <= BOARD_SIZE; x++)
		{
			XY xy = y + x;
			if (!board.is_empty(xy))
			{
				continue;
			}

			int mk[] = { 0, 0, 0, 0 }; // 各色の4方向の石の数
			for (int d : DIR4)
			{
				mk[board[xy + d]]++;
			}
			// 黒の眼
			if (mk[BLACK] > 0 && mk[WHITE] == 0)
			{
				score++;
			}
			// 白の眼
			if (mk[WHITE] > 0 && mk[BLACK] == 0)
			{
				score--;
			}
		}
	}

	final_score = score - KOMI;

	if (final_score > 0) {
		return BLACK;
	}
	else {
		return WHITE;
	}
}

// プレイアウト
int UCTSample::playout(Board& board, const Color color)
{
	int possibles[BOARD_SIZE_MAX * BOARD_SIZE_MAX]; // 動的に確保しない

	// 終局までランダムに打つ
	Color color_tmp = color;
	int pre_xy = -1;
	for (int loop = 0; loop < BOARD_MAX + 200; loop++)
	{
		// 候補手一覧
		int possibles_num = 0;
		for (XY y = BOARD_WIDTH; y < BOARD_MAX - BOARD_WIDTH; y += BOARD_WIDTH)
		{
			for (XY x = 1; x <= BOARD_SIZE; x++)
			{
				XY xy = y + x;
				if (board.is_empty(xy))
				{
					possibles[possibles_num++] = xy;
				}
			}
		}

		int selected_xy;
		int selected_i;
		while (true)
		{
			if (possibles_num == 0)
			{
				selected_xy = PASS;
			}
			else {
				// ランダムで手を選ぶ
				selected_i = random.random() % possibles_num;
				selected_xy = possibles[selected_i];
			}

			// 石を打つ
			MoveResult err = board.move(selected_xy, color_tmp);

			if (err == SUCCESS)
			{
				break;
			}

			// 手を削除
			possibles[selected_i] = possibles[possibles_num - 1];
			possibles_num--;
		}

		// 連続パスで終了
		if (selected_xy == PASS && pre_xy == PASS)
		{
			break;
		}

		// 一つ前の手を保存
		pre_xy = selected_xy;

		// プレイヤー交代
		color_tmp = opponent(color_tmp);
	}

	// 終局 勝敗を返す
	Color win = end_game(board);
	if (win == color)
	{
		return 1;
	}
	else {
		return 0;
	}
}

// UCBからプレイアウトする手を選択
UCTNode* UCTSample::select_node_with_ucb(UCTNode* node)
{
	UCTNode* selected_node;
	double max_ucb = -999;
	for (int i = 0; i < node->child_num; i++)
	{
		UCTNode* child = node->child + i;
		float ucb;
		if (child->playout_num == 0)
		{
			// 未実行
			ucb = FPU + float(random.random()) * FPU / RANDOM_MAX;
		}
		else {
			ucb = float(child->win_num) / child->playout_num + C * sqrtf(logf(node->playout_num_sum) / child->playout_num);
		}

		if (ucb > max_ucb)
		{
			max_ucb = ucb;
			selected_node = child;
		}
	}

	return selected_node;
}

// UCT
int UCTSample::search_uct(Board& board, const Color color, UCTNode* node)
{
	// UCBからプレイアウトする手を選択
	UCTNode* selected_node;
	while (true)
	{
		selected_node = select_node_with_ucb(node);
		MoveResult err = board.move(selected_node->xy, color);
		if (err == SUCCESS)
		{
			break;
		}
		else {
			// 除外
			node->child_num--;
			*selected_node = node->child[node->child_num]; // 値コピー
		}
	}

	int win;

	// 閾値以下の場合プレイアウト
	if (selected_node->playout_num < THR)
	{
		win = 1 - playout(board, opponent(color));
	}
	else {
		if (selected_node->child_num == 0)
		{
			// ノードを展開
			if (selected_node->playout_num == THR && selected_node->expand_node(board))
			{
				win = 1 - search_uct(board, opponent(color), selected_node);
			}
			else {
				// ノードプール不足
				win = 1 - playout(board, opponent(color));
			}
		}
		else {
			win = 1 - search_uct(board, opponent(color), selected_node);
		}
	}

	// 勝率を更新
	selected_node->win_num += win;
	selected_node->playout_num++;
	node->playout_num_sum++;

	return win;
}

// 打つ手を選択
XY UCTSample::select_move(Board& board, Color color)
{
	root = create_root_node();
	root->expand_node(board);

	for (int i = 0; i < PLAYOUT_MAX; i++)
	{
		// 局面コピー
		Board board_tmp = board;

		// UCT
		search_uct(board_tmp, color, root);
	}

	// 最もプレイアウト数が多い手を選ぶ
	UCTNode* best_move;
	int num_max = -999;
	double rate_min = 1; // 勝率
	double rate_max = 0; // 勝率
	for (int i = 0; i < root->child_num; i++)
	{
		UCTNode* child = root->child + i;
		if (child->playout_num > 0)
		{
			int num = child->playout_num;
			if (num > num_max)
			{
				best_move = child;
				num_max = num;
			}

			double rate;
			if (rate_min == 1)
			{
				rate = double(child->win_num) / child->playout_num;
				if (rate < rate_min)
				{
					rate_min = rate;
				}
			}
			if (rate_max == 0)
			{
				rate = double(child->win_num) / child->playout_num;
				if (rate > rate_max)
				{
					rate_max = rate;
				}
			}
		}
	}

	if (rate_min == 1)
	{
		return PASS;
	}
	if (rate_max == 0)
	{
		return PASS;
	}

	return best_move->xy;
}

int UCTSample::get_created_node_cnt()
{
	return node_pool_cnt;
}