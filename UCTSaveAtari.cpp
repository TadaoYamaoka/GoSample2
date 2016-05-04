#include <future>
#include "Random.h"
#include "UCTSaveAtari.h"

#ifndef _DEBUG
const int THREAD_NUM = 8; // 論理コア数
#else
const int THREAD_NUM = 1; // 論理コア数
#endif // !_DEBUG

extern thread_local Random random;

extern UCTNode* create_root_node();
extern UCTNode* create_child_node(const int size);

bool is_atari_save_with_ladder_search(const Board& board, const Color color, const XY xy);

// 展開されたノードがアタリを助ける手か調べる
void check_atari_save(const Board& board, const Color color, UCTNode* node)
{
	for (int i = 0; i < node->child_num; i++)
	{
		node->child[i].is_atari_save = is_atari_save_with_ladder_search(board, color, node->child[i].xy);
	}
}

// シチョウ判定
bool ladder_search(const Board& board, const Color color, const XY xy, Color tmp_board[BOARD_BYTE_MAX], XY liberties[2], const int depth)
{
	if (depth == 0)
	{
		return false;
	}

	// 石を置く
	tmp_board[xy] = color;

	// 石を置いた場所の呼吸点について
	for (int i = 0; i < 2; i++)
	{
		XY xyl = liberties[i];

		// もう一方の場所に敵の石を置く
		tmp_board[liberties[1 - i]] = opponent(color);

		// 敵の石を置いた後のもう一方の呼吸点の呼吸点
		int liberty_num = 0;
		XY liberties_afer[8] = { 0 }; // 大きめに確保
		for (XY d : DIR4)
		{
			XY xyld = xyl + d;

			if (xyld == xy)
			{
				continue;
			}

			if (board.is_empty(xyld) && tmp_board[xyld] == EMPTY)
			{
				liberties_afer[liberty_num++] = xyld;
				continue;
			}

			if (board.is_offboard(xyld))
			{
				continue;
			}

			// もう一方の呼吸点に石を置いた場合、石を取ることができるか
			const Group& group = board.get_group(xyld);
			if (board[xyld] == opponent(color))
			{
				if (group.liberty_num == 1)
				{
					liberties_afer[liberty_num++] = xyld;
				}
			}
			else if (board[xyld] == color)
			{
				// 連結した後の呼吸点

				XY liberties_tmp[4];
				int num = group.get_four_liberties(liberties_tmp);
				for (int j = 0; j < num && liberty_num < 4; j++)
				{
					XY xy_tmp = liberties_tmp[j];
					if (tmp_board[xy_tmp] == EMPTY && xy_tmp != xyl)
					{
						liberties_afer[liberty_num++] = xy_tmp;
					}
				}
			}
		}

		// 呼吸点が1の場合、シチョウ
		if (liberty_num == 1)
		{
			return true;
		}
		else if (liberty_num == 2)
		{
			// 呼吸点が2の場合、次の手を探索
			if (ladder_search(board, color, xyl, tmp_board, liberties_afer, depth - 1))
			{
				return true;
			}
		}

		// この場所はシチョウではない
		// 手を戻す
		tmp_board[liberties[1 - i]] = EMPTY;
	}

	// 手を戻す
	tmp_board[xy] = EMPTY;

	return false;
}

// アタリを助ける手か（シチョウ判定有）
bool is_atari_save_with_ladder_search(const Board& board, const Color color, const XY xy)
{
	if (xy == PASS)
	{
		return false;
	}

	XY liberties[8] = { 0 }; // 大きめに確保

	bool is_atari_save = false;

	int liberty_num = 0;
	for (XY d : DIR4)
	{
		XY xyd = xy + d;

		if (board.is_empty(xyd))
		{
			liberties[liberty_num++] = xyd;
			continue;
		}
		if (board.is_offboard(xyd))
		{
			continue;
		}

		const Group& adjacent_group = board.get_group(xyd);

		if (adjacent_group.color == color)
		{
			// 隣接する自分の連の呼吸点が1か
			if (adjacent_group.liberty_num == 1)
			{
				// アタリを助ける手
				is_atari_save = true;
			}
			else
			{
				// 連結後の呼吸点

				// シチョウ判定を行う場合、連結後の呼吸点を取得
				XY liberties_tmp[4];
				int num = adjacent_group.get_four_liberties(liberties_tmp);
				for (int j = 0; j < num && liberty_num < 4; j++)
				{
					XY xy_tmp = liberties_tmp[j];
					if (xy_tmp != xy)
					{
						liberties[liberty_num++] = xy_tmp;
					}
				}
			}
		}
		else
		{
			if (adjacent_group.liberty_num == 1)
			{
				// 取ることができる
				liberties[liberty_num++] = xyd;

				// 取る連に隣接する自分の連の呼吸点1の場合、アタリを助ける手
				GroupIndex group_idx_tmp = 0;
				for (int j = 0; j < adjacent_group.adjacent.get_part_size(); j++, group_idx_tmp += BIT)
				{
					BitBoardPart adjacent_bitborad = adjacent_group.adjacent.get_bitboard_part(j);
					unsigned long idx;
					while (bit_scan_forward(&idx, adjacent_bitborad))
					{
						GroupIndex group_idx = group_idx_tmp + idx;
						if (board.groups[group_idx].liberty_num == 1)
						{
							return true;
						}

						bit_test_and_reset(&adjacent_bitborad, idx);
					}
				}
			}
		}
	}

	if (is_atari_save)
	{
		if (liberty_num >= 3)
		{
			// アタリを助けた後にも呼吸点が3以上ある(2はシチョウの可能性があるので除外)
			return true;
		}
		else if (liberty_num == 2)
		{
			// シチョウ判定
			Color tmp_board[BOARD_BYTE_MAX] = { 0 };
			if (!ladder_search(board, color, xy, tmp_board, liberties, 10))
			{
				// シチョウではないので、アタリを助ける手とする
				return true;
			}
		}
	}

	return false;
}

// プレイアウト
int UCTSaveAtari::playout(Board& board, const Color color)
{
	int possibles[BOARD_SIZE_MAX * BOARD_SIZE_MAX]; // 動的に確保しない

	// 終局までランダムに打つ
	Color color_tmp = color;
	int pre_xy = -1;
	for (int loop = 0; loop < BOARD_MAX + 200; loop++)
	{
		int selected_xy;
		int selected_i = -1;

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

		// アタリを助ける手を選ばれやすくする
		BitBoard<BOARD_BYTE_MAX> atari_save;
		int atari_save_num = board.get_atari_save(color_tmp, atari_save);
		for (int xy = 0, hit_num = 0; hit_num < atari_save_num; xy++)
		{
			if (atari_save.bit_test(xy))
			{
				if (random.random() < RANDOM_MAX / 2 / atari_save_num)
				{
					selected_i = possibles_num;
					selected_xy = xy;
					break;
				}
			}
		}

		if (selected_i == -1)
		{
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
UCTNode* UCTSaveAtari::select_node_with_ucb(const Board& board, const Color color, UCTNode* node)
{
	UCTNode* selected_node;
	double max_ucb = -999;
	for (int i = 0; i < node->child_num; i++)
	{
		UCTNode* child = node->child + i;

		double bonus = 1.0;
		if (child->xy == PASS)
		{
			// PASSに低いボーナス
			bonus = 0.01;
		}
		else if (child->is_atari_save) {
			// アタリを助ける手にボーナス
			bonus = 10.0;
		}

		double ucb;
		if (child->playout_num == 0)
		{
			// 未実行
			ucb = FPU + double(random.random()) * FPU / RANDOM_MAX;
			ucb *= bonus;
		}
		else {
			ucb = double(child->win_num) / child->playout_num + C * sqrt(log(node->playout_num_sum) / child->playout_num) * bonus;
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
int UCTSaveAtari::search_uct(Board& board, const Color color, UCTNode* node)
{
	// UCBからプレイアウトする手を選択
	UCTNode* selected_node;
	while (true)
	{
		selected_node = select_node_with_ucb(board, color, node);
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
				// 展開されたノードがアタリを助ける手か調べる
				check_atari_save(board, color, selected_node);

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

void UCTSaveAtari::search_uct_root(Board& board, const Color color, UCTNode* node, const std::map<UCTNode*, UCTNode*>& copynodemap)
{
	// UCBからプレイアウトする手を選択
	// rootノードはアトミックに更新するためUCB計算ではロックしない
	UCTNode* selected_node = select_node_with_ucb(board, color, node);

	// rootでは全て合法手なのでエラーチェックはしない
	board.move_legal(selected_node->xy, color);

	// コピーされたノードに変換
	UCTNode* selected_node_copy = copynodemap.at(selected_node);

	int win;

	// 閾値以下の場合プレイアウト(全スレッドの合計値)
	if (selected_node->playout_num < THR)
	{
		win = 1 - playout(board, opponent(color));
	}
	else {
		if (selected_node_copy->child_num == 0)
		{
			// ノードを展開
			if (selected_node_copy->expand_node(board))
			{
				// 展開されたノードがアタリを助ける手か調べる
				check_atari_save(board, color, selected_node_copy);

				win = 1 - search_uct(board, opponent(color), selected_node_copy);
			}
			else {
				// ノードプール不足
				win = 1 - playout(board, opponent(color));
			}
		}
		else {
			win = 1 - search_uct(board, opponent(color), selected_node_copy);
		}
	}

	// 勝率を更新(アトミックに加算)
	_InterlockedExchangeAdd(&selected_node->win_num, win);
	_InterlockedIncrement(&selected_node->playout_num);
	_InterlockedIncrement(&node->playout_num_sum);
}

XY UCTSaveAtari::select_move(Board& board, Color color)
{
	UCTNode* root = create_root_node();
	this->root = root;

	// ノードを展開(合法手のみ)
	expand_root_node(board, color, root);

	// 展開されたノードがアタリを助ける手か調べる
	check_atari_save(board, color, root);

	// root並列化
	std::thread th[THREAD_NUM];
	for (int th_i = 0; th_i < THREAD_NUM; th_i++)
	{
		th[th_i] = std::thread([root, board, color] {
			// rootノードのコピー(子ノードのポインタと新たに作成したノードを対応付ける)
			std::map<UCTNode*, UCTNode*> copynodemap;
			UCTNode* copynode = create_child_node(root->child_num);
			if (copynode == nullptr)
			{
				fprintf(stderr, "node pool too small\n");
				return;
			}
			for (int i = 0; i < root->child_num; i++)
			{
				copynode[i].xy = root->child[i].xy;
				copynode[i].playout_num = 0;
				copynode[i].playout_num_sum = 0;
				copynode[i].child_num = 0;

				copynodemap.insert({ root->child + i, copynode + i });
			}

			for (int i = 0; i < PLAYOUT_MAX / THREAD_NUM; i++)
			{
				// 局面コピー
				Board board_tmp = board;

				// UCT
				search_uct_root(board_tmp, color, root, copynodemap);
			}
		});
	}

	// スレッド終了待機
	for (int th_i = 0; th_i < THREAD_NUM; th_i++)
	{
		th[th_i].join();
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