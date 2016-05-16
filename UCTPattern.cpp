#include <string>
#include "UCTPattern.h"
#include "Random.h"
#include "learn/Hash.h"

using namespace std;

#ifndef _DEBUG
const int THREAD_NUM = 8; // 論理コア数
#else
const int THREAD_NUM = 1; // 論理コア数
#endif // !_DEBUG

const float CPUCT = 20.0f; // PUCT定数
const float C = 1.0f; // UCB定数
const float FPU = 1.0f; // First Play Urgency
const int THR = 15; // ノード展開の閾値

extern thread_local Random random;

extern UCTNode* create_root_node();
extern UCTNode* create_child_node(const int size);

extern void check_atari_save(const Board& board, const Color color, UCTNode* node);

// rollout policyの重み
RolloutPolicyWeightHash rpw;

// tree policyの重み
TreePolicyWeightHash tpw;

void set_pattern_hash(float* hash_tbl, const ResponsePatternVal& val, const float weight)
{
	hash_tbl[get_hash_key_response_pattern(val)] = weight;
}

void set_pattern_hash(float* hash_tbl, const NonResponsePatternVal& val, const float weight)
{
	hash_tbl[get_hash_key_nonresponse_pattern(val)] = weight;
}

void set_pattern_hash(float* hash_tbl, const Diamond12PatternVal& val, const float weight)
{
	hash_tbl[get_hash_key_diamond12_pattern(val)] = weight;
}

template<typename V>
void set_rotated_pattern_hash(float* hash_tbl, const V& val, const float weight)
{
	set_pattern_hash(hash_tbl, val, weight);

	// 90度回転
	V rot = val.rotate();
	set_pattern_hash(hash_tbl, rot, weight);

	// 180度回転
	rot = val.rotate();
	set_pattern_hash(hash_tbl, rot, weight);

	// 270度回転
	rot = val.rotate();
	set_pattern_hash(hash_tbl, rot, weight);

	// 上下反転
	rot = val.vmirror();
	set_pattern_hash(hash_tbl, rot, weight);

	// 90度回転
	rot = rot.rotate();
	set_pattern_hash(hash_tbl, rot, weight);

	// 左右反転
	rot = val.hmirror();
	set_pattern_hash(hash_tbl, rot, weight);

	// 90度回転
	rot = rot.rotate();
	set_pattern_hash(hash_tbl, rot, weight);
}

void load_weight(const wchar_t* dirpath)
{
	init_hash_table_and_weight(9999999619ull);

	wstring path(dirpath);

	// 重み読み込み
	// rollout policy
	FILE* fp_weight = _wfopen((path + L"/rollout.bin").c_str(), L"rb");
	if (fp_weight == NULL)
	{
		fprintf(stderr, "rollout.bin open error.\n");
		exit(1);
	}
	fread(&rpw.save_atari_weight, sizeof(rpw.save_atari_weight), 1, fp_weight);
	fread(&rpw.neighbour_weight, sizeof(rpw.neighbour_weight), 1, fp_weight);
	//fread(&rpw.response_match_weight, sizeof(rpw.response_match_weight), 1, fp_weight);
	int num;
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		ResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		set_rotated_pattern_hash(rpw.response_pattern_weight, val, weight);
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		NonResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		set_rotated_pattern_hash(rpw.nonresponse_pattern_weight, val, weight);
	}
	fclose(fp_weight);

	// tree policy
	fp_weight = _wfopen((path + L"/tree.bin").c_str(), L"rb");
	if (fp_weight == NULL)
	{
		fprintf(stderr, "tree.bin open error.\n");
		exit(1);
	}
	fread(&tpw.save_atari_weight, sizeof(tpw.save_atari_weight), 1, fp_weight);
	fread(&tpw.neighbour_weight, sizeof(tpw.neighbour_weight), 1, fp_weight);
	//fread(&tpw.response_match_weight, sizeof(tpw.response_match_weight), 1, fp_weight);
	fread(&tpw.self_atari_weight, sizeof(tpw.self_atari_weight), 1, fp_weight);
	fread(&tpw.last_move_distance_weight, sizeof(tpw.last_move_distance_weight), 1, fp_weight);
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		ResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		set_rotated_pattern_hash(tpw.response_pattern_weight, val, weight);
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		NonResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		set_rotated_pattern_hash(tpw.nonresponse_pattern_weight, val, weight);
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		Diamond12PatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		set_rotated_pattern_hash(tpw.diamond12_pattern_weight, val, weight);
	}
	fclose(fp_weight);
}

// 展開されたノードの着手確率をtree policyを使用して算出
void compute_tree_policy(const Board& board, Color color, UCTNode* parent)
{
	float e_weight_sum = 0;

	ResponsePatternVal response_base;
	if (board.pre_xy[0] > PASS)
	{
		response_base = get_diamon12_pattern_val(board, board.pre_xy[0], color);
	}

	for (int i = 0; i < parent->child_num; i++)
	{
		UCTNode& node = parent->child[i];

		if (node.xy == PASS)
		{
			node.probability = 0;
			continue;
		}

		// レスポンスパターン
		const ResponsePatternVal response_val = response_pattern(board, node.xy, color, response_base);

		// ノンレスポンスパターン
		const NonResponsePatternVal nonresponse_val = nonresponse_pattern(board, node.xy, color);

		// Diamond12パターン
		const Diamond12PatternVal diamond12_val = diamond12_pattern(board, node.xy, color);

		// 重みの線形和
		float tree_weight_sum = tpw.nonresponse_pattern_weight[get_hash_key_nonresponse_pattern(nonresponse_val)];
		if (response_val != 0)
		{
			//tree_weight_sum += tpw.response_match_weight;
			tree_weight_sum += tpw.response_pattern_weight[get_hash_key_response_pattern(response_val)];
		}
		// アタリを助ける手か
		if (board.is_atari_save_with_ladder_search(color, node.xy))
		{
			tree_weight_sum += tpw.save_atari_weight;
		}
		// 直前の手に隣接する手か
		if (is_neighbour(board, node.xy))
		{
			tree_weight_sum += tpw.neighbour_weight;
		}
		// アタリになる手か
		if (board.is_self_atari(color, node.xy))
		{
			tree_weight_sum += tpw.self_atari_weight;
		}
		// 直前2手からの距離
		for (int move = 0; move < 2; move++)
		{
			if (board.pre_xy[move] != PASS)
			{
				XY distance = get_distance(node.xy, board.pre_xy[move]);
				if (distance >= sizeof(tpw.last_move_distance_weight[0]) / sizeof(tpw.last_move_distance_weight[0][0]))
				{
					distance = sizeof(tpw.last_move_distance_weight[0]) / sizeof(tpw.last_move_distance_weight[0][0]) - 1;
				}
				tree_weight_sum += tpw.last_move_distance_weight[move][distance];
			}
		}
		// 12-point diamondパターン
		tree_weight_sum += tpw.diamond12_pattern_weight[get_hash_key_diamond12_pattern(diamond12_val)];

		// 各手のsoftmaxを計算
		node.probability = expf(tree_weight_sum);

		e_weight_sum += node.probability;
	}

	// 合計で割って確率にする
	for (int i = 0; i < parent->child_num; i++)
	{
		UCTNode& node = parent->child[i];

		node.probability /= e_weight_sum;
	}
}

// プレイアウト
int UCTPattern::playout(Board& board, const Color color)
{
	struct Possible
	{
		XY xy;
		int e_weight;
	} possibles[BOARD_SIZE_MAX * BOARD_SIZE_MAX]; // 動的に確保しない

	BitBoard<BOARD_BYTE_MAX> atari_save;

	// Non-responseパターン評価値(保存用)
	float non_response_weight_board[BOARD_BYTE_MAX] = { 0 };

	// 終局までランダムに打つ
	Color color_tmp = color;
	int pre_xy = -1;
	for (int loop = 0; loop < BOARD_MAX + 200; loop++)
	{
		int selected_xy;
		int e_weight_sum = 0;

		// アタリを助ける手
		board.get_atari_save(color_tmp, atari_save);

		// 直前に変更のあった連の周辺の評価値を初期化する
		for (int i = 0; i < board.pre_changed_group_num; i++)
		{
			const Group& changed_group = board.groups[board.pre_changed_group[i]];
			for (int j = 0; j < changed_group.stone_num; j++)
			{
				const XY xy = changed_group.stone[j];

				// 周囲8マスを更新対象
				non_response_weight_board[xy - BOARD_WIDTH - 1] = 0;
				non_response_weight_board[xy - BOARD_WIDTH] = 0;
				non_response_weight_board[xy - BOARD_WIDTH + 1] = 0;
				non_response_weight_board[xy - 1] = 0;
				non_response_weight_board[xy + 1] = 0;
				non_response_weight_board[xy + BOARD_WIDTH - 1] = 0;
				non_response_weight_board[xy + BOARD_WIDTH] = 0;
				non_response_weight_board[xy + BOARD_WIDTH + 1] = 0;
			}
		}

		for (int i = 0; i < board.pre_removed_group_num; i++)
		{
			const Group& removed_group = board.groups[board.pre_removed_group[i]];
			for (int j = 0; j < removed_group.stone_num; j++)
			{
				// 削除された位置を更新対象
				const XY xy = removed_group.stone[j];
				non_response_weight_board[xy] = 0;
			}
		}

		// レスポンスパターンで共通となる12-point diamondパターンを計算
		ResponsePatternVal response_base;
		if (board.pre_xy[0] > PASS)
		{
			response_base = get_diamon12_pattern_val(board, board.pre_xy[0], color_tmp);
		}

		// 候補手一覧(合法手チェックなし)
		int possibles_num = 0;
		for (XY y = BOARD_WIDTH; y < BOARD_MAX - BOARD_WIDTH; y += BOARD_WIDTH)
		{
			for (XY x = 1; x <= BOARD_SIZE; x++)
			{
				const XY xy = y + x;
				if (board.is_empty(xy))
				{
					float weight_sum;
					// 確率算出

					// 重みの線形和
					// Non-response pattern
					// 初回もしくは直前に変更のあった連の周辺のみ更新する
					if (non_response_weight_board[xy] == 0)
					{
						const NonResponsePatternVal nonresponse_val = nonresponse_pattern(board, xy, color_tmp);
						non_response_weight_board[xy] = rpw.nonresponse_pattern_weight[get_hash_key_nonresponse_pattern(nonresponse_val)];
					}
					weight_sum = non_response_weight_board[xy];

					// Response pattern
					const ResponsePatternVal response_val = response_pattern(board, xy, color_tmp, response_base);
					if (response_val != 0)
					{
						//weight_sum += rpw.response_match_weight;
						weight_sum += rpw.response_pattern_weight[get_hash_key_response_pattern(response_val)];

						// 直前の手に隣接する手か
						if (is_neighbour(board, xy))
						{
							weight_sum += rpw.neighbour_weight;
						}
					}
					// アタリを助ける手か
					if (atari_save.bit_test(xy))
					{
						weight_sum += rpw.save_atari_weight;
					}

					// 各手のsoftmaxを計算
					const int e_weight = expf(weight_sum) * 1000; // 1000倍して整数にする
					e_weight_sum += e_weight;

					possibles[possibles_num].xy = xy;
					possibles[possibles_num].e_weight = e_weight;

					possibles_num++;
				}
			}
		}

		while (true)
		{
			int selected_i;
			if (possibles_num == 0)
			{
				// 合法手がない場合、パス
				selected_xy = PASS;
			}
			else {
				// 確率に応じて手を選択
				selected_i = possibles_num - 1;
				selected_xy = possibles[selected_i].xy;
				int e_weight_tmp = 0;
				int r = random.random() % e_weight_sum;
				for (int i = 0; i < possibles_num - 1; i++)
				{
					e_weight_tmp += possibles[i].e_weight;
					if (r < e_weight_tmp)
					{
						selected_i = i;
						selected_xy = possibles[i].xy;
						break;
					}
				}
			}

			// 石を打つ
			const MoveResult err = board.move(selected_xy, color_tmp);

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
	const Color win = end_game(board);
	if (win == color)
	{
		return 1;
	}
	else {
		return 0;
	}
}

// UCBからプレイアウトする手を選択
UCTNode* UCTPattern::select_node_with_ucb(const Board& board, const Color color, UCTNode* node)
{
	UCTNode* selected_node;
	float max_ucb = -999;
	for (int i = 0; i < node->child_num; i++)
	{
		UCTNode* child = node->child + i;

		float ucb;
		if (child->playout_num < 3)
		{
			// FPU
			ucb = FPU + float(random.random()) * FPU / RANDOM_MAX;
			//printf("xy = %d, probability = %f\n", child->xy, child->probability);
		}
		else if (child->xy != PASS && child->playout_num < THR) // 閾値以下の場合tree policyを使用
		{
			// PUCT
			ucb = float(child->win_num) / child->playout_num + CPUCT * child->probability * sqrtf(logf(node->playout_num_sum)) / (1 + child->playout_num);
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
int UCTPattern::search_uct(Board& board, const Color color, UCTNode* node)
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
				// 展開されたノードの着手確率をtree policyを使用して算出
				compute_tree_policy(board, opponent(color), selected_node);

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

void UCTPattern::search_uct_root(Board& board, const Color color, UCTNode* node, const std::map<UCTNode*, UCTNode*>& copynodemap)
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
				// 展開されたノードの着手確率をtree policyを使用して算出
				compute_tree_policy(board, opponent(color), selected_node_copy);

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

XY UCTPattern::select_move(Board& board, Color color)
{
	UCTNode* root = create_root_node();
	this->root = root;

	// ノードを展開(合法手のみ)
	expand_root_node(board, color, root);

	// 展開されたノードの着手確率をtree policyを使用して算出
	compute_tree_policy(board, color, root);

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