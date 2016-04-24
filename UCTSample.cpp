#include <math.h>
#include "Random.h"
#include "UCTSample.h"
#include "Debug.h"

const int FPU = 10; // First Play Urgency
const double C = 1.0; // UCB�萔
const int THR = 20; // �m�[�h�W�J��臒l
int PLAYOUT_MAX = 2000;

const int NODE_MAX = 10000;
UCTNode node_pool[NODE_MAX]; // �m�[�h�v�[��(�������̂��ߓ��I�Ɋm�ۂ��Ȃ�)
UCTNode* p_node_pool;

UCTNode* create_root_node()
{
	p_node_pool = node_pool;
	p_node_pool->playout_num_sum = 0;
	p_node_pool->child_num = 0;
	return p_node_pool;
}

UCTNode* create_node(const XY xy)
{
	if (p_node_pool - node_pool >= NODE_MAX - 1)
	{
		return nullptr;
	}
	p_node_pool++;
	p_node_pool->xy = xy;
	p_node_pool->playout_num = 0;
	p_node_pool->playout_num_sum = 0;
	p_node_pool->win_num = 0;
	p_node_pool->child_num = 0;
	return p_node_pool;
}

// �m�[�h�W�J
bool UCTNode::expand_node(const Board& board)
{
	for (XY xy = BOARD_WIDTH + 1; xy < BOARD_MAX - BOARD_WIDTH; xy++)
	{
		if (board.is_empty(xy))
		{
			create_node(xy);
			child_num++;
		}
	}
	// PASS��ǉ�
	child = create_node(PASS);
	if (child == nullptr)
	{
		child_num = 0;
		return false;
	}
	child_num++;

	child -= (child_num - 1); // �擪�̃|�C���^
	return true;
}

// �I�� ���s��Ԃ�
Color end_game(const Board& board)
{
	// �������[���Ő�����
	int score = board.stone_num[BLACK] - board.stone_num[WHITE];
	int eye_num = BOARD_STONE_MAX - (board.stone_num[BLACK] + board.stone_num[WHITE]);
	double final_score = score - KOMI;

	// ��𐔂��Ȃ��Ă����s�����܂�ꍇ
	if (final_score - eye_num > 0)
	{
		// �Ⴊ�S�Ĕ����Ƃ��Ă����̏���
		return BLACK;
	}
	else if (final_score + eye_num < 0)
	{
		// �Ⴊ�S�č����Ƃ��Ă����̏���
		return WHITE;
	}

	//debug_print_board(board);

	// ��𐔂���
	for (XY xy = BOARD_WIDTH + 1; xy < BOARD_MAX - BOARD_WIDTH; xy++)
	{
		int mk[] = { 0, 0, 0, 0 }; // �e�F��4�����̐΂̐�
		for (int d : DIR4)
		{
			mk[board[xy + d]]++;
		}
		// ���̊�
		if (mk[BLACK] > 0 && mk[WHITE] == 0)
		{
			score++;
		}
		// ���̊�
		if (mk[WHITE] > 0 && mk[BLACK] == 0)
		{
			score--;
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

// �v���C�A�E�g
int playout(Board& board, UCTNode* node, const Color color)
{
	int possibles[BOARD_SIZE_MAX * BOARD_SIZE_MAX]; // ���I�Ɋm�ۂ��Ȃ�

	// �I�ǂ܂Ń����_���ɑł�
	Color color_tmp = color;
	int pre_xy = -1;
	for (int loop = 0; loop < BOARD_MAX + 200; loop++)
	{
		// ����ꗗ
		int possibles_num = 0;
		for (XY xy = BOARD_WIDTH + 1; xy < BOARD_MAX - BOARD_WIDTH; xy++)
		{
			if (board.is_empty(xy))
			{
				possibles[possibles_num++] = xy;
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
				// �����_���Ŏ��I��
				selected_i = random() % possibles_num;
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
UCTNode* select_node_with_ucb(UCTNode* node)
{
	UCTNode* selected_node;
	double max_ucb = -999;
	for (int i = 0; i < node->child_num; i++)
	{
		UCTNode* child = node->child + i;
		double ucb;
		if (child->playout_num == 0)
		{
			// �����s
			ucb = FPU + double(random()) * FPU / RANDOM_MAX;
		}
		else {
			ucb = double(child->win_num) / child->playout_num + C * sqrt(log(node->playout_num_sum) / child->playout_num);
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
int search_uct(Board& board, const Color color, UCTNode* node)
{
	// UCB����v���C�A�E�g������I��
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
			// ���O
			node->child_num--;
			*selected_node = node->child[node->child_num]; // �l�R�s�[
		}
	}

	int win;

	// 臒l�ȉ��̏ꍇ�v���C�A�E�g
	if (selected_node->playout_num < THR)
	{
		win = 1 - playout(board, selected_node, opponent(color));
	}
	else {
		// �m�[�h��W�J
		if (selected_node->child_num == 0)
		{
			if (selected_node->expand_node(board))
			{
				win = 1 - search_uct(board, opponent(color), selected_node);
			}
			else {
				// �m�[�h�v�[���s��
				win = 1 - playout(board, selected_node, opponent(color));
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

// �ł��I��
XY UCTSample::select_move(Board& board, Color color)
{
	root = create_root_node();
	root->expand_node(board);

	for (int i = 0; i < PLAYOUT_MAX; i++)
	{
		// �ǖʃR�s�[
		Board board_tmp = board;

		// UCT
		search_uct(board_tmp, color, root);
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

int UCTSample::get_created_node()
{
	return (int)(p_node_pool - node_pool + 1);
}