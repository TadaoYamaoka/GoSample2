#include <string>
#include "UCTPattern.h"
#include "Random.h"
#include "learn/Hash.h"

using namespace std;

#ifndef _DEBUG
const int THREAD_NUM = 8; // �_���R�A��
#else
const int THREAD_NUM = 1; // �_���R�A��
#endif // !_DEBUG

const float CPUCT = 20.0f; // PUCT�萔
const float C = 1.0f; // UCB�萔
const float FPU = 1.0f; // First Play Urgency
const int THR = 15; // �m�[�h�W�J��臒l

extern thread_local Random random;

extern UCTNode* create_root_node();
extern UCTNode* create_child_node(const int size);

extern void check_atari_save(const Board& board, const Color color, UCTNode* node);

// rollout policy�̏d��
RolloutPolicyWeightHash rpw;

// tree policy�̏d��
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

	// 90�x��]
	V rot = val.rotate();
	set_pattern_hash(hash_tbl, rot, weight);

	// 180�x��]
	rot = val.rotate();
	set_pattern_hash(hash_tbl, rot, weight);

	// 270�x��]
	rot = val.rotate();
	set_pattern_hash(hash_tbl, rot, weight);

	// �㉺���]
	rot = val.vmirror();
	set_pattern_hash(hash_tbl, rot, weight);

	// 90�x��]
	rot = rot.rotate();
	set_pattern_hash(hash_tbl, rot, weight);

	// ���E���]
	rot = val.hmirror();
	set_pattern_hash(hash_tbl, rot, weight);

	// 90�x��]
	rot = rot.rotate();
	set_pattern_hash(hash_tbl, rot, weight);
}

void load_weight(const wchar_t* dirpath)
{
	init_hash_table_and_weight(9999999619ull);

	wstring path(dirpath);

	// �d�ݓǂݍ���
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

// �W�J���ꂽ�m�[�h�̒���m����tree policy���g�p���ĎZ�o
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

		// ���X�|���X�p�^�[��
		const ResponsePatternVal response_val = response_pattern(board, node.xy, color, response_base);

		// �m�����X�|���X�p�^�[��
		const NonResponsePatternVal nonresponse_val = nonresponse_pattern(board, node.xy, color);

		// Diamond12�p�^�[��
		const Diamond12PatternVal diamond12_val = diamond12_pattern(board, node.xy, color);

		// �d�݂̐��`�a
		float tree_weight_sum = tpw.nonresponse_pattern_weight[get_hash_key_nonresponse_pattern(nonresponse_val)];
		if (response_val != 0)
		{
			//tree_weight_sum += tpw.response_match_weight;
			tree_weight_sum += tpw.response_pattern_weight[get_hash_key_response_pattern(response_val)];
		}
		// �A�^����������肩
		if (board.is_atari_save_with_ladder_search(color, node.xy))
		{
			tree_weight_sum += tpw.save_atari_weight;
		}
		// ���O�̎�ɗאڂ���肩
		if (is_neighbour(board, node.xy))
		{
			tree_weight_sum += tpw.neighbour_weight;
		}
		// �A�^���ɂȂ�肩
		if (board.is_self_atari(color, node.xy))
		{
			tree_weight_sum += tpw.self_atari_weight;
		}
		// ���O2�肩��̋���
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
		// 12-point diamond�p�^�[��
		tree_weight_sum += tpw.diamond12_pattern_weight[get_hash_key_diamond12_pattern(diamond12_val)];

		// �e���softmax���v�Z
		node.probability = expf(tree_weight_sum);

		e_weight_sum += node.probability;
	}

	// ���v�Ŋ����Ċm���ɂ���
	for (int i = 0; i < parent->child_num; i++)
	{
		UCTNode& node = parent->child[i];

		node.probability /= e_weight_sum;
	}
}

// �v���C�A�E�g
int UCTPattern::playout(Board& board, const Color color)
{
	struct Possible
	{
		XY xy;
		int e_weight;
	} possibles[BOARD_SIZE_MAX * BOARD_SIZE_MAX]; // ���I�Ɋm�ۂ��Ȃ�

	BitBoard<BOARD_BYTE_MAX> atari_save;

	// Non-response�p�^�[���]���l(�ۑ��p)
	float non_response_weight_board[BOARD_BYTE_MAX] = { 0 };

	// �I�ǂ܂Ń����_���ɑł�
	Color color_tmp = color;
	int pre_xy = -1;
	for (int loop = 0; loop < BOARD_MAX + 200; loop++)
	{
		int selected_xy;
		int e_weight_sum = 0;

		// �A�^�����������
		board.get_atari_save(color_tmp, atari_save);

		// ���O�ɕύX�̂������A�̎��ӂ̕]���l������������
		for (int i = 0; i < board.pre_changed_group_num; i++)
		{
			const Group& changed_group = board.groups[board.pre_changed_group[i]];
			for (int j = 0; j < changed_group.stone_num; j++)
			{
				const XY xy = changed_group.stone[j];

				// ����8�}�X���X�V�Ώ�
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
				// �폜���ꂽ�ʒu���X�V�Ώ�
				const XY xy = removed_group.stone[j];
				non_response_weight_board[xy] = 0;
			}
		}

		// ���X�|���X�p�^�[���ŋ��ʂƂȂ�12-point diamond�p�^�[�����v�Z
		ResponsePatternVal response_base;
		if (board.pre_xy[0] > PASS)
		{
			response_base = get_diamon12_pattern_val(board, board.pre_xy[0], color_tmp);
		}

		// ����ꗗ(���@��`�F�b�N�Ȃ�)
		int possibles_num = 0;
		for (XY y = BOARD_WIDTH; y < BOARD_MAX - BOARD_WIDTH; y += BOARD_WIDTH)
		{
			for (XY x = 1; x <= BOARD_SIZE; x++)
			{
				const XY xy = y + x;
				if (board.is_empty(xy))
				{
					float weight_sum;
					// �m���Z�o

					// �d�݂̐��`�a
					// Non-response pattern
					// ����������͒��O�ɕύX�̂������A�̎��ӂ̂ݍX�V����
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

						// ���O�̎�ɗאڂ���肩
						if (is_neighbour(board, xy))
						{
							weight_sum += rpw.neighbour_weight;
						}
					}
					// �A�^����������肩
					if (atari_save.bit_test(xy))
					{
						weight_sum += rpw.save_atari_weight;
					}

					// �e���softmax���v�Z
					const int e_weight = expf(weight_sum) * 1000; // 1000�{���Đ����ɂ���
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
				// ���@�肪�Ȃ��ꍇ�A�p�X
				selected_xy = PASS;
			}
			else {
				// �m���ɉ����Ď��I��
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

			// �΂�ł�
			const MoveResult err = board.move(selected_xy, color_tmp);

			if (err == SUCCESS)
			{
				break;
			}

			// ����폜
			possibles[selected_i] = possibles[possibles_num - 1];
			possibles_num--;
		}

		// �A���p�X�ŏI��
		if (selected_xy == PASS && pre_xy == PASS)
		{
			break;
		}

		// ��O�̎��ۑ�
		pre_xy = selected_xy;

		// �v���C���[���
		color_tmp = opponent(color_tmp);
	}

	// �I�� ���s��Ԃ�
	const Color win = end_game(board);
	if (win == color)
	{
		return 1;
	}
	else {
		return 0;
	}
}

// UCB����v���C�A�E�g������I��
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
		else if (child->xy != PASS && child->playout_num < THR) // 臒l�ȉ��̏ꍇtree policy���g�p
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
	// UCB����v���C�A�E�g������I��
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
			// ���O
			node->child_num--;
			*selected_node = node->child[node->child_num]; // �l�R�s�[
		}
	}

	int win;

	// 臒l�ȉ��̏ꍇ�v���C�A�E�g
	if (selected_node->playout_num < THR)
	{
		win = 1 - playout(board, opponent(color));
	}
	else {
		if (selected_node->child_num == 0)
		{
			// �m�[�h��W�J
			if (selected_node->playout_num == THR && selected_node->expand_node(board))
			{
				// �W�J���ꂽ�m�[�h�̒���m����tree policy���g�p���ĎZ�o
				compute_tree_policy(board, opponent(color), selected_node);

				win = 1 - search_uct(board, opponent(color), selected_node);
			}
			else {
				// �m�[�h�v�[���s��
				win = 1 - playout(board, opponent(color));
			}
		}
		else {
			win = 1 - search_uct(board, opponent(color), selected_node);
		}
	}

	// �������X�V
	selected_node->win_num += win;
	selected_node->playout_num++;
	node->playout_num_sum++;

	return win;
}

void UCTPattern::search_uct_root(Board& board, const Color color, UCTNode* node, const std::map<UCTNode*, UCTNode*>& copynodemap)
{
	// UCB����v���C�A�E�g������I��
	// root�m�[�h�̓A�g�~�b�N�ɍX�V���邽��UCB�v�Z�ł̓��b�N���Ȃ�
	UCTNode* selected_node = select_node_with_ucb(board, color, node);

	// root�ł͑S�č��@��Ȃ̂ŃG���[�`�F�b�N�͂��Ȃ�
	board.move_legal(selected_node->xy, color);

	// �R�s�[���ꂽ�m�[�h�ɕϊ�
	UCTNode* selected_node_copy = copynodemap.at(selected_node);

	int win;

	// 臒l�ȉ��̏ꍇ�v���C�A�E�g(�S�X���b�h�̍��v�l)
	if (selected_node->playout_num < THR)
	{
		win = 1 - playout(board, opponent(color));
	}
	else {
		if (selected_node_copy->child_num == 0)
		{
			// �m�[�h��W�J
			if (selected_node_copy->expand_node(board))
			{
				// �W�J���ꂽ�m�[�h�̒���m����tree policy���g�p���ĎZ�o
				compute_tree_policy(board, opponent(color), selected_node_copy);

				win = 1 - search_uct(board, opponent(color), selected_node_copy);
			}
			else {
				// �m�[�h�v�[���s��
				win = 1 - playout(board, opponent(color));
			}
		}
		else {
			win = 1 - search_uct(board, opponent(color), selected_node_copy);
		}
	}

	// �������X�V(�A�g�~�b�N�ɉ��Z)
	_InterlockedExchangeAdd(&selected_node->win_num, win);
	_InterlockedIncrement(&selected_node->playout_num);
	_InterlockedIncrement(&node->playout_num_sum);
}

XY UCTPattern::select_move(Board& board, Color color)
{
	UCTNode* root = create_root_node();
	this->root = root;

	// �m�[�h��W�J(���@��̂�)
	expand_root_node(board, color, root);

	// �W�J���ꂽ�m�[�h�̒���m����tree policy���g�p���ĎZ�o
	compute_tree_policy(board, color, root);

	// root����
	std::thread th[THREAD_NUM];
	for (int th_i = 0; th_i < THREAD_NUM; th_i++)
	{
		th[th_i] = std::thread([root, board, color] {
			// root�m�[�h�̃R�s�[(�q�m�[�h�̃|�C���^�ƐV���ɍ쐬�����m�[�h��Ή��t����)
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
				// �ǖʃR�s�[
				Board board_tmp = board;

				// UCT
				search_uct_root(board_tmp, color, root, copynodemap);
			}
		});
	}

	// �X���b�h�I���ҋ@
	for (int th_i = 0; th_i < THREAD_NUM; th_i++)
	{
		th[th_i].join();
	}

	// �ł��v���C�A�E�g�����������I��
	UCTNode* best_move;
	int num_max = -999;
	double rate_min = 1; // ����
	double rate_max = 0; // ����
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