#include <atomic>
#include "UCTSLPolicy.h"
#include "Random.h"
#include "log.h"

#ifndef _DEBUG
const int THREAD_NUM = 7; // スレッド数(DCNN用に1つ空けておく)
#else
const int THREAD_NUM = 1; // スレッド数
#endif // !_DEBUG

const float CPUCT = 10.0f; // PUCT定数
const int THR = 15; // ノード展開の閾値

extern thread_local Random random;

extern UCTNode* create_root_node();
extern UCTNode* create_child_node(const int size);

// DCNN
CNN cnn;

// 要求キュー
atomic<int> dcnn_request_cnt = 0;
atomic<int> dcnn_request_prepared = 0;
UCTNode* dcnn_request_node[minibatch_size]; // 子ノードのprobabilityに計算結果を格納
float dcnn_srcData[minibatch_size][feature_num][19][19]; // 入力特徴

// DCNN実行回数
int dcnn_exec_cnt;
int dcnn_depth_sum;

// UCBからプレイアウトする手を選択
UCTNode* UCTSLPolicy::select_node_with_ucb(const Board& board, const Color color, UCTNode* node)
{
	UCTNode* selected_node;
	float max_ucb = -999;
	for (int i = 0; i < node->child_num - 1; i++)
	{
		UCTNode* child = node->child + i;

		float ucb;
		if (node->playout_num_sum == 0)
		{
			ucb = child->probability;
		}
		else
		{
			float q;
			if (child->playout_num == 0) {
				q = 0.5f;
			}
			else {
				q = float(child->win_num) / child->playout_num;
			}
			// PUCT
			ucb = q + CPUCT * child->probability * sqrtf(node->playout_num_sum) / (1 + child->playout_num);
		}

		if (ucb > max_ucb)
		{
			max_ucb = ucb;
			selected_node = child;
		}
	}

	return selected_node;
}

// DCNN要求
void request_dcnn(const Board& board, const Color color, UCTNode* node, bool& dcnn_requested, const int depth)
{
	// SL policy未要求の場合
	if (!node->dcnn_requested && !dcnn_requested)
	{
		// キューに空きがある場合
		int no = dcnn_request_cnt.fetch_add(1);
		if (no < minibatch_size)
		{
			// キューに入れる
			dcnn_request_node[no] = node;

			// 入力特徴
			for (int y = 0; y < 19; y++)
			{
				for (int x = 0; x < 19; x++)
				{
					Color c = board[get_xy(x + 1, y + 1)];
					if (color == WHITE)
					{
						c = opponent(c);
					}
					switch (c)
					{
					case BLACK:
						dcnn_srcData[no][0][y][x] = 1.0f;
						dcnn_srcData[no][1][y][x] = 0.0f;
						dcnn_srcData[no][2][y][x] = 0.0f;
						break;
					case WHITE:
						dcnn_srcData[no][0][y][x] = 0.0f;
						dcnn_srcData[no][1][y][x] = 1.0f;
						dcnn_srcData[no][2][y][x] = 0.0f;
						break;
					case EMPTY:
						dcnn_srcData[no][0][y][x] = 0.0f;
						dcnn_srcData[no][1][y][x] = 0.0f;
						dcnn_srcData[no][2][y][x] = 1.0f;
						break;
					}
					dcnn_srcData[no][3][y][x] = 1.0f;
				}
			}

			// 要求済みにする
			node->dcnn_requested = true;

			// 準備済み
			dcnn_request_prepared++;

			dcnn_depth_sum += depth;
		}
		// キューに空きがなかった場合も、親ノードからDCNNを実行するため枝ノードでは要求済みとする
		dcnn_requested = true;
	}
}

// UCT
int UCTSLPolicy::search_uct(Board& board, const Color color, UCTNode* node, bool& dcnn_requested, int depth)
{
	// DCNN要求
	request_dcnn(board, color, node, dcnn_requested, depth);

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
				// 展開されたノードの着手確率をtree policyを使用して算出
				compute_tree_policy(board, opponent(color), selected_node);

				win = 1 - search_uct(board, opponent(color), selected_node, dcnn_requested, depth + 1);
			}
			else {
				// ノードプール不足
				win = 1 - playout(board, opponent(color));
			}
		}
		else {
			win = 1 - search_uct(board, opponent(color), selected_node, dcnn_requested, depth + 1);
		}
	}

	// 勝率を更新
	selected_node->win_num += win;
	selected_node->playout_num++;
	node->playout_num_sum++;

	return win;
}

void UCTSLPolicy::search_uct_root(Board& board, const Color color, UCTNode* node, UCTNode* copychild)
{
	// UCBからプレイアウトする手を選択
	// rootノードはアトミックに更新するためUCB計算ではロックしない
	UCTNode* selected_node = select_node_with_ucb(board, color, node);

	// rootでは全て合法手なのでエラーチェックはしない
	board.move_legal(selected_node->xy, color);

	// コピーされたノードに変換
	UCTNode* selected_node_copy = copychild + (selected_node - node->child);

	bool dcnn_requested = false;

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
				// 展開されたノードの着手確率をtree policyを使用して算出
				compute_tree_policy(board, opponent(color), selected_node_copy);

				win = 1 - search_uct(board, opponent(color), selected_node_copy, dcnn_requested, 1);
			}
			else {
				// ノードプール不足
				win = 1 - playout(board, opponent(color));
			}
		}
		else {
			win = 1 - search_uct(board, opponent(color), selected_node_copy, dcnn_requested, 1);
		}
	}

	// 勝率を更新(アトミックに加算)
	_InterlockedExchangeAdd(&selected_node->win_num, win);
	_InterlockedIncrement(&selected_node->playout_num);
	_InterlockedIncrement(&node->playout_num_sum);
}

XY UCTSLPolicy::select_move(Board& board, Color color)
{
	UCTNode* root = create_root_node();
	this->root = root;

	// ノードを展開(合法手のみ)
	expand_root_node(board, color, root);

	// 展開されたノードの着手確率をtree policyを使用して算出
	compute_tree_policy(board, color, root);

	// DCNN要求
	bool dcnn_requested = false;
	dcnn_request_prepared = 0;
	dcnn_request_cnt = 0;
	request_dcnn(board, color, root, dcnn_requested, 0);

	/*
	// 1手目はtree policyを使わずにDCNNの計算結果を使う
	// 順伝播
	float dcnn_dstData[minibatch_size][1][19][19]; // 出力
	cnn.forward(dcnn_srcData, dcnn_dstData);

	// 結果をノードに反映
	// 候補手の着手確率を設定(PASSを除く)
	for (int j = 0; j < dcnn_request_node[0]->child_num - 1; j++)
	{
		const XY xy = dcnn_request_node[0]->child->xy;
		const XY x = get_x(xy);
		const XY y = get_y(xy);
		dcnn_request_node[0]->child[j].probability = dcnn_dstData[0][0][y - 1][x - 1];
	}

	// キューを空にする
	dcnn_request_prepared = 0;
	dcnn_request_cnt = 0;
	*/

	// 実行回数カウント
	dcnn_exec_cnt = 0;
	dcnn_depth_sum = 0;

	// DCNN用スレッド
	bool dcnn_exit = false;
	std::thread th_dcnn = std::thread([root, &board, color, &dcnn_exit] {
		float dcnn_dstData[minibatch_size][1][19][19]; // 出力

		while (!dcnn_exit)
		{
			// キューの準備ができているか
			if (dcnn_request_cnt >= minibatch_size && dcnn_request_prepared == minibatch_size)
			{
				// 順伝播
				cnn.forward(dcnn_srcData, dcnn_dstData);

				//dump_data(logfp, "dcnn_srcData", (float*)dcnn_srcData, sizeof(dcnn_srcData) / sizeof(float));
				//dump_data(logfp, "dcnn_dstData", (float*)dcnn_dstData, sizeof(dcnn_dstData) / sizeof(float));

				// 結果をノードに反映
				for (int i = 0; i < minibatch_size; i++)
				{
					// 候補手の着手確率を設定(PASSを除く)
					for (int j = 0; j < dcnn_request_node[i]->child_num - 1; j++)
					{
						const XY xy = dcnn_request_node[i]->child->xy;
						const XY x = get_x(xy);
						const XY y = get_y(xy);
						dcnn_request_node[i]->child[j].probability = dcnn_dstData[i][0][y - 1][x - 1];
					}
				}

				// キューを空にする
				dcnn_request_prepared = 0;
				dcnn_request_cnt = 0;

				// 実行回数カウント
				dcnn_exec_cnt++;
			}
			else
			{
				std::this_thread::yield();
			}
		}
	});

	// root並列化
	std::thread th[THREAD_NUM];
	for (int th_i = 0; th_i < THREAD_NUM; th_i++)
	{
		th[th_i] = std::thread([root, &board, color] {
			// rootの子ノードのコピー
			UCTNode* copychild = create_child_node(root->child_num);
			if (copychild == nullptr)
			{
				fprintf(stderr, "node pool too small\n");
				return;
			}

			for (int i = 0; i < root->child_num; i++)
			{
				copychild[i].xy = root->child[i].xy;
				copychild[i].playout_num = 0;
				copychild[i].playout_num_sum = 0;
				copychild[i].child_num = 0;
				copychild[i].dcnn_requested = false;
			}

			for (int i = 0; i < PLAYOUT_MAX / THREAD_NUM; i++)
			{
				// 局面コピー
				Board board_tmp = board;

				// UCT
				search_uct_root(board_tmp, color, root, copychild);
			}
		});
	}

	// スレッド終了待機
	for (int th_i = 0; th_i < THREAD_NUM; th_i++)
	{
		th[th_i].join();
	}
	dcnn_exit = true;
	th_dcnn.join();

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
			rate = double(child->win_num) / child->playout_num;
			if (rate < rate_min)
			{
				rate_min = rate;
			}
			if (rate > rate_max)
			{
				rate_max = rate;
			}
		}
	}

	if (rate_min >= 0.99)
	{
		return PASS;
	}
	if (rate_max <= 0.01)
	{
		return PASS;
	}

	return best_move->xy;
}