#include "UCTPattern.h"
#include "Random.h"

#ifndef _DEBUG
const int THREAD_NUM = 8; // �_���R�A��
#else
const int THREAD_NUM = 1; // �_���R�A��
#endif // !_DEBUG

extern thread_local Random random;

extern UCTNode* create_root_node();
extern UCTNode* create_child_node(const int size);

extern void check_atari_save(const Board& board, const Color color, UCTNode* node);

// rollout policy�̏d��
RolloutPolicyWeight rpw;

void load_weight(const char* filepath)
{
	// �d�ݓǂݍ���
	FILE* fp_weight = fopen(filepath, "rb");
	fread(&rpw.save_atari_weight, sizeof(rpw.save_atari_weight), 1, fp_weight);
	fread(&rpw.neighbour_weight, sizeof(rpw.neighbour_weight), 1, fp_weight);
	fread(&rpw.response_match_weight, sizeof(rpw.response_match_weight), 1, fp_weight);
	int num;
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		ResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		rpw.response_pattern_weight.insert({ val, weight });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		NonResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		rpw.nonresponse_pattern_weight.insert({ val, weight });
	}
	fclose(fp_weight);
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

				const XY x = get_x(xy);
				const XY y = get_y(xy);
				XY x_min = x - 2;
				if (x_min < 1)
				{
					x_min = 1;
				}
				XY x_max = x + 2;
				if (x_max > BOARD_SIZE)
				{
					x_max = BOARD_SIZE;
				}
				XY y_min = y - 2;
				if (y_min < 1)
				{
					y_min = 1;
				}
				XY y_max = y + 2;
				if (y_max > BOARD_SIZE)
				{
					y_max = BOARD_SIZE;
				}
				for (XY y = y_min; y <= y_max; y++)
				{
					for (XY x = x_min; x <= x_min; x++)
					{
						non_response_weight_board[get_xy(x, y)] = 0;
					}
				}
			}
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
						const auto itr = rpw.nonresponse_pattern_weight.find(nonresponse_val);
						if (itr != rpw.nonresponse_pattern_weight.end())
						{
							non_response_weight_board[xy] = itr->second;
						}
					}
					weight_sum = non_response_weight_board[xy];

					// Response pattern
					const ResponsePatternVal response_val = response_pattern(board, xy, color_tmp);
					if (response_val != 0)
					{
						//weight_sum += response_match_weight;
						const auto itr = rpw.response_pattern_weight.find(response_val);
						if (itr != rpw.response_pattern_weight.end())
						{
							weight_sum += itr->second;
						}

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
	double max_ucb = -999;
	for (int i = 0; i < node->child_num; i++)
	{
		UCTNode* child = node->child + i;

		double bonus = 1.0;
		if (child->xy == PASS)
		{
			// PASS�ɒႢ�{�[�i�X
			bonus = 0.01;
		}
		else if (child->is_atari_save) {
			// �A�^�����������Ƀ{�[�i�X
			bonus = 10.0;
		}

		double ucb;
		if (child->playout_num == 0)
		{
			// �����s
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
				// �W�J���ꂽ�m�[�h���A�^����������肩���ׂ�
				check_atari_save(board, color, selected_node);

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
				// �W�J���ꂽ�m�[�h���A�^����������肩���ׂ�
				check_atari_save(board, color, selected_node_copy);

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

	// �W�J���ꂽ�m�[�h���A�^����������肩���ׂ�
	check_atari_save(board, color, root);

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