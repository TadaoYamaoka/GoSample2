#pragma once

#include "UCTPattern.h"
#include "Predict/CNN.h"

extern void compute_tree_policy(const Board& board, Color color, UCTNode* parent);
extern CNN cnn;
extern int dcnn_exec_cnt;

class UCTSLPolicy : public UCTPattern
{
protected:
	static void search_uct_root(Board& board, const Color color, UCTNode* node, UCTNode* copychild);
	static int search_uct(Board& board, const Color color, UCTNode* node, bool& dcnn_requested);
	static UCTNode* select_node_with_ucb(const Board& board, const Color color, UCTNode* node);

public:
	virtual XY select_move(Board& board, Color color);
};