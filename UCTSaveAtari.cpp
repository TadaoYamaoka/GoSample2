#include <future>
#include "Random.h"
#include "UCTSaveAtari.h"

#ifndef _DEBUG
const int THREAD_NUM = 8; // �_���R�A��
#else
const int THREAD_NUM = 1; // �_���R�A��
#endif // !_DEBUG

const float C = 1.0; // UCB�萔
const int FPU = 10; // First Play Urgency
const int THR = 15; // �m�[�h�W�J��臒l

extern thread_local Random random;

extern UCTNode* create_root_node();
extern UCTNode* create_child_node(const int size);

// �W�J���ꂽ�m�[�h���A�^����������肩���ׂ�
void check_atari_save(const Board& board, const Color color, UCTNode* node)
{
	for (int i = 0; i < node->child_num; i++)
	{
		node->child[i].is_atari_save = board.is_atari_save_with_ladder_search(color, node->child[i].xy);
	}
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
			ucb = FPU + float(random.random()) * FPU / RANDOM_MAX;
			ucb *= bonus;
		}
		else {
			ucb = float(child->win_num) / child->playout_num + C * sqrtf(logf(node->playout_num_sum) / child->playout_num) * bonus;
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

void UCTSaveAtari::search_uct_root(Board& board, const Color color, UCTNode* node, UCTNode* copychild)
{
	// UCB����v���C�A�E�g������I��
	// root�m�[�h�̓A�g�~�b�N�ɍX�V���邽��UCB�v�Z�ł̓��b�N���Ȃ�
	UCTNode* selected_node = select_node_with_ucb(board, color, node);

	// root�ł͑S�č��@��Ȃ̂ŃG���[�`�F�b�N�͂��Ȃ�
	board.move_legal(selected_node->xy, color);

	// �R�s�[���ꂽ�m�[�h�ɕϊ�
	UCTNode* selected_node_copy = copychild + (selected_node - node->child);

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
		th[th_i] = std::thread([root, &board, color] {
			// root�̎q�m�[�h�̃R�s�[
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
			}

			for (int i = 0; i < PLAYOUT_MAX / THREAD_NUM; i++)
			{
				// �ǖʃR�s�[
				Board board_tmp = board;

				// UCT
				search_uct_root(board_tmp, color, root, copychild);
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
		return RESIGN;
	}

	return best_move->xy;
}