#pragma once
#include <map>
#include "UCTSample.h"

class UCTParallel :	public UCTSample
{
protected:
	static void search_uct_root(Board& board, const Color color, UCTNode* node, UCTNode* copychild);
	static void expand_root_node(const Board& board, const Color color, UCTNode* root);

public:
	virtual XY select_move(Board& board, Color color);
};
