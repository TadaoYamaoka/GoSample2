#pragma once
#include "Player.h"

extern int PLAYOUT_MAX;

class UCTNode
{
public:
	XY xy;
	int playout_num;
	int playout_num_sum;
	int win_num;
	int child_num; // �q�m�[�h�̐�
	UCTNode* child; // �q�m�[�h

	bool expand_node(const Board& board);
};

class UCTSample : public Player
{
public:
	UCTNode* root;
	virtual XY select_move(Board& board, Color color);

	int get_created_node();
};
