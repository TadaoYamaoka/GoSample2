#pragma once
#include "Player.h"

extern int PLAYOUT_MAX;

class UCTNode
{
public:
	XY xy;
	volatile long playout_num;
	volatile long playout_num_sum;
	volatile long win_num;
	int child_num; // �q�m�[�h�̐�
	UCTNode* child; // �q�m�[�h

	bool is_atari_save; // �A�^����������肩

	float probability; // tree policy or sl policy
	bool dcnn_requested; // SL policy�v�Z�ς݃t���O

	bool expand_node(const Board& board);
};

class UCTSample : public Player
{
protected:
	static int playout(Board& board, const Color color);
	static int search_uct(Board& board, const Color color, UCTNode* node);
	static UCTNode* select_node_with_ucb(UCTNode* node);
	static Color end_game(const Board& board);

public:
	UCTNode* root;
	virtual XY select_move(Board& board, Color color);

	int get_created_node_cnt();
};
