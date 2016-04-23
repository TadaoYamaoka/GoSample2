#pragma once

#include "Board.h"

class Player
{
public:
	virtual XY select_move(Board& board, Color color) = 0;
};

