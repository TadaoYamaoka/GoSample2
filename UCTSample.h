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
	int child_num; // 子ノードの数
	UCTNode* child; // 子ノード

	bool expand_node(const Board& board);
};

class UCTSample : public Player
{
public:
	UCTNode* root;
	virtual XY select_move(Board& board, Color color);

	int get_created_node();
};
