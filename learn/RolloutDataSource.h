#pragma once

#include "../Board.h"

#pragma pack(push, 1)
class RolloutDataSource
{
public:
	XY xy;
	XY pre_xy[2];
	BitBoard<19 * 19> player_color;
	BitBoard<19 * 19> opponent_color;
};
#pragma pack(pop)
