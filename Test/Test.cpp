#include <stdio.h>
#include <time.h>
#include "../Debug.h"
#include "../Board.h"
#include "../UCTSample.h"
#include "../UCTParallel.h"
#include "../UCTSaveAtari.h"

UCTSaveAtari player;

void init_board(Board& board, Color* test_board, const int boardsize)
{
	for (int i = 0; i < boardsize * boardsize; i++)
	{
		XY xy = get_xy(i % boardsize + 1, i / boardsize + 1);
		if (test_board[i] != EMPTY)
		{
			board.move(xy, test_board[i], false);
		}
	}
}

void assert(const XY xy, const XY expected)
{
	if (xy == expected)
	{
		printf("success\n");
	}
	else {
		printf("fail\n");
	}
}

void print_child(UCTNode* root)
{
	for (int i = 0; i < root->child_num; i++)
	{
		UCTNode* child = root->child + i;
		printf("xy = %d, x,y = %d,%d : win = %d : playout_num = %d\n", child->xy, get_x(child->xy), get_y(child->xy), child->win_num, child->playout_num);
	}
}

void test_001()
{
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		2, 2, 2, 2, 2, 1, 0, 1, 1, // 1
		2, 2, 2, 2, 2, 1, 1, 1, 1, // 2 
		2, 2, 2, 2, 2, 1, 1, 0, 1, // 3
		2, 2, 2, 2, 2, 1, 1, 1, 1, // 4
		2, 2, 2, 2, 2, 1, 1, 2, 2, // 5
		1, 1, 1, 1, 2, 1, 1, 2, 2, // 6
		1, 1, 0, 0, 0, 0, 0, 2, 2, // 7
		2, 2, 2, 2, 2, 2, 2, 2, 2, // 8
		2, 2, 2, 2, 2, 2, 2, 2, 2  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	XY xy = player.select_move(board, BLACK);

	print_child(player.root);
	printf("xy = %d, x,y = %d,%d\n", xy, get_x(xy), get_y(xy));

	assert(xy, get_xy(5, 7));
}

void test_002() {
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		2, 2, 2, 2, 2, 1, 0, 1, 1, // 1
		2, 2, 2, 2, 2, 1, 1, 1, 1, // 2 
		2, 2, 2, 2, 2, 1, 1, 0, 1, // 3
		2, 2, 2, 2, 2, 1, 1, 1, 1, // 4
		2, 2, 2, 2, 2, 1, 1, 1, 1, // 5
		1, 1, 1, 1, 2, 1, 1, 2, 2, // 6
		1, 1, 0, 0, 0, 0, 0, 2, 0, // 7
		2, 2, 2, 2, 2, 2, 2, 2, 2, // 8
		2, 2, 2, 2, 2, 2, 2, 2, 0  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	XY xy = player.select_move(board, WHITE);

	print_child(player.root);
	printf("xy = %d, x,y = %d,%d\n", xy, get_x(xy), get_y(xy));

	assert(xy, get_xy(5, 7));
}

void test_003() {
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		0, 2, 1, 1, 0, 1, 1, 1, 1, // 1
		2, 2, 2, 1, 1, 1, 1, 1, 1, // 2 
		2, 0, 2, 2, 2, 1, 1, 2, 1, // 3
		2, 2, 2, 1, 1, 1, 1, 2, 2, // 4
		0, 2, 1, 1, 0, 2, 1, 2, 0, // 5
		2, 0, 2, 1, 1, 2, 1, 2, 2, // 6
		0, 2, 0, 2, 2, 1, 1, 1, 0, // 7
		2, 2, 2, 2, 0, 2, 0, 1, 1, // 8
		0, 2, 2, 0, 2, 0, 2, 2, 0  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	XY xy = player.select_move(board, BLACK);

	print_child(player.root);
	printf("xy = %d, x,y = %d,%d\n", xy, get_x(xy), get_y(xy));

	assert(xy, PASS);
}

void test_004() {
	// アタリから逃げるケース

	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 1
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 2 
		0, 0, 0, 0, 0, 1, 1, 2, 0, // 3
		0, 0, 0, 0, 2, 1, 2, 0, 0, // 4
		0, 0, 0, 0, 2, 1, 1, 2, 2, // 5
		0, 0, 0, 0, 1, 2, 2, 1, 1, // 6
		0, 0, 0, 0, 0, 2, 1, 2, 0, // 7
		0, 0, 0, 0, 0, 0, 1, 0, 0, // 8
		0, 0, 0, 0, 0, 0, 0, 0, 0  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	XY xy = player.select_move(board, BLACK);

	print_child(player.root);
	printf("xy = %d, x,y = %d,%d\n", xy, get_x(xy), get_y(xy));
}

int main()
{
	//test_001();
	//test_002();
	//test_003();
	test_004();
	return 0;
}