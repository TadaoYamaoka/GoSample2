#include <future>
#include "Random.h"
#include "UCTSaveAtari.h"

const int THREAD_NUM = 8; // 論理コア数

extern thread_local Random random;

extern UCTNode* create_root_node();
extern UCTNode* create_child_node(const int size);

// アタリを助ける手を取得
int UCTSaveAtari::get_atari_save(const Board& board, const Color color, BitBoard<BOARD_BYTE_MAX>& atari_save)
{
	int atari_save_num = 0;

	// 自分の色の連の一覧から呼吸点が1の場所について
	for (GroupIndex idx = board.groups.begin(); idx != board.groups.end(); idx = board.groups.next(idx))
	{
		const Group& group = board.groups[idx];

		// TODO 死活探索

			// 呼吸点に打った場合に助けることができるか
		if (group.color == color && group.liberty_num == 1)
		{
			// 呼吸点の場所
			XY xy = group.get_first_liberty();

			int liberty = 0;
			for (XY d : DIR4)
			{
				XY xyd = xy + d;
				if (board.is_empty(xyd))
				{
					liberty++;
					continue;
				}
				if (board.is_offboard(xyd))
				{
					continue;
				}

				const Group& adjacent_group = board.get_group(xyd);
				if (adjacent_group.color == color)
				{
					if (board.board[xyd] != idx)
					{
						// 連結後の呼吸点
						liberty += adjacent_group.liberty_num - 1;
					}
				}
				else
				{
					if (adjacent_group.liberty_num == 1)
					{
						// 取ることができる
						liberty++;
					}
				}
			}
			if (liberty >= 2)
			{
				// アタリを助けた後にも呼吸点が2以上ある
				atari_save.bit_test_and_set(xy);
			}

			// 隣接する連の呼吸点が1の場合助けることができる
			GroupIndex group_idx_tmp = 0;
			for (int j = 0; j < group.adjacent.get_part_size(); j++, group_idx_tmp += BIT)
			{
				BitBoardPart adjacent_bitborad = group.adjacent.get_bitboard_part(j);
				unsigned long idx;
				while (bit_scan_forward(&idx, adjacent_bitborad))
				{
					GroupIndex group_idx = group_idx_tmp + idx;
					if (board.groups[group_idx].liberty_num == 1)
					{
						// 呼吸点の場所
						atari_save.bit_test_and_set(board.groups[group_idx].get_first_liberty());
					}

					bit_test_and_reset(&adjacent_bitborad, idx);
				}
			}
		}
	}
	return atari_save_num;
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
		for (XY xy = BOARD_WIDTH + 1; xy < BOARD_MAX - BOARD_WIDTH; xy++)
		{
			if (board.is_empty(xy))
			{
				possibles[possibles_num++] = xy;
			}
		}

		// アタリを助ける手を選ばれやすくする
		BitBoard<BOARD_BYTE_MAX> atari_save;
		atari_save.set_all_zero();
		int atari_save_num = get_atari_save(board, color_tmp, atari_save);
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
	// アタリを助ける手を選ばれやすくする
	BitBoard<BOARD_BYTE_MAX> atari_save;
	atari_save.set_all_zero();
	int atari_save_num = get_atari_save(board, color, atari_save);

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
		else if (atari_save.bit_test(child->xy)) {
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

	board.move(selected_node->xy, color);

	// rootでは全て合法手なのでエラーチェックはしない

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