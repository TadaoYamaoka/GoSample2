#pragma once

#include "Board.h"

#ifdef _DEBUG
#include <stdio.h>

inline void debug_print_board(const Board& board)
{
	// ƒwƒbƒ_[
	printf("  ");
	for (int x = 1; x <= BOARD_SIZE; x++)
	{
		printf("%d", x % 10);
	}
	printf("\n");

	for (int y = 1; y <= BOARD_SIZE; y++)
	{
		printf("%d ", y % 10);
		for (int x = 1; x <= BOARD_SIZE; x++)
		{
			XY xy = get_xy(x, y);
			printf("%d", board[xy]);
		}
		printf("\n");
	}
	printf("\n");
}
#else
inline void debug_print_board(const Board& board) {}
#endif // _DEBUG
