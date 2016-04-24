#pragma once
#include "UCTSample.h"

class UCTParallel :	public UCTSample
{
public:
	virtual XY select_move(Board& board, Color color);
};
