#pragma once
#include "Player.h"
class Human : public Player
{
	XY xy;
public:
	virtual XY select_move(Board& board, Color color);
	void set_xy(XY xy);
};
