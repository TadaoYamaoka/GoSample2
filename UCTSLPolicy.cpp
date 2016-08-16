#include <atomic>
#include "UCTSLPolicy.h"
#include "Random.h"
#include "log.h"

#ifndef _DEBUG
const int THREAD_NUM = 7; // �X���b�h��(DCNN�p��1�󂯂Ă���)
#else
const int THREAD_NUM = 1; // �X���b�h��
#endif // !_DEBUG

const float CPUCT = 10.0f; // PUCT�萔
const int THR = 15; // �m�[�h�W�J��臒l

extern thread_local Random random;

extern UCTNode* create_root_node();
extern UCTNode* create_child_node(const int size);

// DCNN
CNN cnn;

// �v���L���[
atomic<int> dcnn_request_cnt = 0;
atomic<int> dcnn_request_prepared = 0;
UCTNode* dcnn_request_node[minibatch_size]; // �q�m�[�h��probability�Ɍv�Z���ʂ��i�[
float dcnn_srcData[minibatch_size][feature_num][19][19]; // ���͓���

// DCNN���s��
int dcnn_exec_cnt;
int dcnn_depth_sum;

// UCB����v���C�A�E�g������I��
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

// DCNN�v��
void request_dcnn(const Board& board, const Color color, UCTNode* node, bool& dcnn_requested, const int depth)
{
	// SL policy���v���̏ꍇ
	if (!node->dcnn_requested && !dcnn_requested)
	{
		// �L���[�ɋ󂫂�����ꍇ
		int no = dcnn_request_cnt.fetch_add(1);
		if (no < minibatch_size)
		{
			// �L���[�ɓ����
			dcnn_request_node[no] = node;

			// ���͓���
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

			// �v���ς݂ɂ���
			node->dcnn_requested = true;

			// �����ς�
			dcnn_request_prepared++;

			dcnn_depth_sum += depth;
		}
		// �L���[�ɋ󂫂��Ȃ������ꍇ���A�e�m�[�h����DCNN�����s���邽�ߎ}�m�[�h�ł͗v���ς݂Ƃ���
		dcnn_requested = true;
	}
}

// UCT
int UCTSLPolicy::search_uct(Board& board, const Color color, UCTNode* node, bool& dcnn_requested, int depth)
{
	// DCNN�v��
	request_dcnn(board, color, node, dcnn_requested, depth);

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

				win = 1 - search_uct(board, opponent(color), selected_node, dcnn_requested, depth + 1);
			}
			else {
				// �m�[�h�v�[���s��
				win = 1 - playout(board, opponent(color));
			}
		}
		else {
			win = 1 - search_uct(board, opponent(color), selected_node, dcnn_requested, depth + 1);
		}
	}

	// �������X�V
	selected_node->win_num += win;
	selected_node->playout_num++;
	node->playout_num_sum++;

	return win;
}

void UCTSLPolicy::search_uct_root(Board& board, const Color color, UCTNode* node, UCTNode* copychild)
{
	// UCB����v���C�A�E�g������I��
	// root�m�[�h�̓A�g�~�b�N�ɍX�V���邽��UCB�v�Z�ł̓��b�N���Ȃ�
	UCTNode* selected_node = select_node_with_ucb(board, color, node);

	// root�ł͑S�č��@��Ȃ̂ŃG���[�`�F�b�N�͂��Ȃ�
	board.move_legal(selected_node->xy, color);

	// �R�s�[���ꂽ�m�[�h�ɕϊ�
	UCTNode* selected_node_copy = copychild + (selected_node - node->child);

	bool dcnn_requested = false;

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

				win = 1 - search_uct(board, opponent(color), selected_node_copy, dcnn_requested, 1);
			}
			else {
				// �m�[�h�v�[���s��
				win = 1 - playout(board, opponent(color));
			}
		}
		else {
			win = 1 - search_uct(board, opponent(color), selected_node_copy, dcnn_requested, 1);
		}
	}

	// �������X�V(�A�g�~�b�N�ɉ��Z)
	_InterlockedExchangeAdd(&selected_node->win_num, win);
	_InterlockedIncrement(&selected_node->playout_num);
	_InterlockedIncrement(&node->playout_num_sum);
}

XY UCTSLPolicy::select_move(Board& board, Color color)
{
	UCTNode* root = create_root_node();
	this->root = root;

	// �m�[�h��W�J(���@��̂�)
	expand_root_node(board, color, root);

	// �W�J���ꂽ�m�[�h�̒���m����tree policy���g�p���ĎZ�o
	compute_tree_policy(board, color, root);

	// DCNN�v��
	bool dcnn_requested = false;
	dcnn_request_prepared = 0;
	dcnn_request_cnt = 0;
	request_dcnn(board, color, root, dcnn_requested, 0);

	/*
	// 1��ڂ�tree policy���g�킸��DCNN�̌v�Z���ʂ��g��
	// ���`�d
	float dcnn_dstData[minibatch_size][1][19][19]; // �o��
	cnn.forward(dcnn_srcData, dcnn_dstData);

	// ���ʂ��m�[�h�ɔ��f
	// ����̒���m����ݒ�(PASS������)
	for (int j = 0; j < dcnn_request_node[0]->child_num - 1; j++)
	{
		const XY xy = dcnn_request_node[0]->child->xy;
		const XY x = get_x(xy);
		const XY y = get_y(xy);
		dcnn_request_node[0]->child[j].probability = dcnn_dstData[0][0][y - 1][x - 1];
	}

	// �L���[����ɂ���
	dcnn_request_prepared = 0;
	dcnn_request_cnt = 0;
	*/

	// ���s�񐔃J�E���g
	dcnn_exec_cnt = 0;
	dcnn_depth_sum = 0;

	// DCNN�p�X���b�h
	bool dcnn_exit = false;
	std::thread th_dcnn = std::thread([root, &board, color, &dcnn_exit] {
		float dcnn_dstData[minibatch_size][1][19][19]; // �o��

		while (!dcnn_exit)
		{
			// �L���[�̏������ł��Ă��邩
			if (dcnn_request_cnt >= minibatch_size && dcnn_request_prepared == minibatch_size)
			{
				// ���`�d
				cnn.forward(dcnn_srcData, dcnn_dstData);

				//dump_data(logfp, "dcnn_srcData", (float*)dcnn_srcData, sizeof(dcnn_srcData) / sizeof(float));
				//dump_data(logfp, "dcnn_dstData", (float*)dcnn_dstData, sizeof(dcnn_dstData) / sizeof(float));

				// ���ʂ��m�[�h�ɔ��f
				for (int i = 0; i < minibatch_size; i++)
				{
					// ����̒���m����ݒ�(PASS������)
					for (int j = 0; j < dcnn_request_node[i]->child_num - 1; j++)
					{
						const XY xy = dcnn_request_node[i]->child->xy;
						const XY x = get_x(xy);
						const XY y = get_y(xy);
						dcnn_request_node[i]->child[j].probability = dcnn_dstData[i][0][y - 1][x - 1];
					}
				}

				// �L���[����ɂ���
				dcnn_request_prepared = 0;
				dcnn_request_cnt = 0;

				// ���s�񐔃J�E���g
				dcnn_exec_cnt++;
			}
			else
			{
				std::this_thread::yield();
			}
		}
	});

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
				copychild[i].dcnn_requested = false;
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
	dcnn_exit = true;
	th_dcnn.join();

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