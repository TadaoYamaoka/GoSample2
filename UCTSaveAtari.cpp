#include <future>
#include "Random.h"
#include "UCTSaveAtari.h"

#ifndef _DEBUG
const int THREAD_NUM = 8; // �_���R�A��
#else
const int THREAD_NUM = 1; // �_���R�A��
#endif // !_DEBUG

extern thread_local Random random;

extern UCTNode* create_root_node();
extern UCTNode* create_child_node(const int size);

bool is_atari_save_with_ladder_search(const Board& board, const Color color, const XY xy);

// �W�J���ꂽ�m�[�h���A�^����������肩���ׂ�
void check_atari_save(const Board& board, const Color color, UCTNode* node)
{
	for (int i = 0; i < node->child_num; i++)
	{
		node->child[i].is_atari_save = is_atari_save_with_ladder_search(board, color, node->child[i].xy);
	}
}

// �V�`���E����
bool ladder_search(const Board& board, const Color color, const XY xy, Color tmp_board[BOARD_BYTE_MAX], XY liberties[2], const int depth)
{
	if (depth == 0)
	{
		return false;
	}

	// �΂�u��
	tmp_board[xy] = color;

	// �΂�u�����ꏊ�̌ċz�_�ɂ���
	for (int i = 0; i < 2; i++)
	{
		XY xyl = liberties[i];

		// ��������̏ꏊ�ɓG�̐΂�u��
		tmp_board[liberties[1 - i]] = opponent(color);

		// �G�̐΂�u������̂�������̌ċz�_�̌ċz�_
		int liberty_num = 0;
		XY liberties_afer[8] = { 0 }; // �傫�߂Ɋm��
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

			// ��������̌ċz�_�ɐ΂�u�����ꍇ�A�΂���邱�Ƃ��ł��邩
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
				// �A��������̌ċz�_

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

		// �ċz�_��1�̏ꍇ�A�V�`���E
		if (liberty_num == 1)
		{
			return true;
		}
		else if (liberty_num == 2)
		{
			// �ċz�_��2�̏ꍇ�A���̎��T��
			if (ladder_search(board, color, xyl, tmp_board, liberties_afer, depth - 1))
			{
				return true;
			}
		}

		// ���̏ꏊ�̓V�`���E�ł͂Ȃ�
		// ���߂�
		tmp_board[liberties[1 - i]] = EMPTY;
	}

	// ���߂�
	tmp_board[xy] = EMPTY;

	return false;
}

// �A�^����������肩�i�V�`���E����L�j
bool is_atari_save_with_ladder_search(const Board& board, const Color color, const XY xy)
{
	if (xy == PASS)
	{
		return false;
	}

	XY liberties[8] = { 0 }; // �傫�߂Ɋm��

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
			// �אڂ��鎩���̘A�̌ċz�_��1��
			if (adjacent_group.liberty_num == 1)
			{
				// �A�^�����������
				is_atari_save = true;
			}
			else
			{
				// �A����̌ċz�_

				// �V�`���E������s���ꍇ�A�A����̌ċz�_���擾
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
				// ��邱�Ƃ��ł���
				liberties[liberty_num++] = xyd;

				// ���A�ɗאڂ��鎩���̘A�̌ċz�_1�̏ꍇ�A�A�^�����������
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
			// �A�^������������ɂ��ċz�_��3�ȏ゠��(2�̓V�`���E�̉\��������̂ŏ��O)
			return true;
		}
		else if (liberty_num == 2)
		{
			// �V�`���E����
			Color tmp_board[BOARD_BYTE_MAX] = { 0 };
			if (!ladder_search(board, color, xy, tmp_board, liberties, 10))
			{
				// �V�`���E�ł͂Ȃ��̂ŁA�A�^�����������Ƃ���
				return true;
			}
		}
	}

	return false;
}

// �v���C�A�E�g
int UCTSaveAtari::playout(Board& board, const Color color)
{
	int possibles[BOARD_SIZE_MAX * BOARD_SIZE_MAX]; // ���I�Ɋm�ۂ��Ȃ�

	// �I�ǂ܂Ń����_���ɑł�
	Color color_tmp = color;
	int pre_xy = -1;
	for (int loop = 0; loop < BOARD_MAX + 200; loop++)
	{
		int selected_xy;
		int selected_i = -1;

		// ����ꗗ
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

		// �A�^������������I�΂�₷������
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
					// �����_���Ŏ��I��
					selected_i = random.random() % possibles_num;
					selected_xy = possibles[selected_i];
				}

				// �΂�ł�
				MoveResult err = board.move(selected_xy, color_tmp);

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
		}

		// ��O�̎��ۑ�
		pre_xy = selected_xy;

		// �v���C���[���
		color_tmp = opponent(color_tmp);
	}

	// �I�� ���s��Ԃ�
	Color win = end_game(board);
	if (win == color)
	{
		return 1;
	}
	else {
		return 0;
	}
}

// UCB����v���C�A�E�g������I��
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
int UCTSaveAtari::search_uct(Board& board, const Color color, UCTNode* node)
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

void UCTSaveAtari::search_uct_root(Board& board, const Color color, UCTNode* node, const std::map<UCTNode*, UCTNode*>& copynodemap)
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

XY UCTSaveAtari::select_move(Board& board, Color color)
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