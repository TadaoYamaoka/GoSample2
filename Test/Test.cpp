#include <stdio.h>
#include <time.h>
#include <regex>
#include "../Debug.h"
#include "../Board.h"
#include "../learn/Sgf.h"
#include "../UCTSample.h"
#include "../UCTParallel.h"
#include "../UCTSaveAtari.h"
#include "../UCTPattern.h";

using namespace std;

UCTSample player;

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

// SGF��ǂݍ���Ń{�[�h������������
void load_sgf(Board& board, char* sgf_text)
{
	// ;�ŋ�؂�
	char* next = strtok(sgf_text, ";");

	// 1�ڂ͓ǂݔ�΂�
	next = strtok(NULL, ";");

	// �T�C�Y�擾
	regex re("SZ\\[\\(d+)\\]");
	cmatch ma;
	if (regex_match(next, ma, re))
	{
		int size = atoi(ma.str(1).c_str());
		board.init(size);
	}
	else
	{
		fprintf(stderr, "sgf error : size\n");
		return;
	}

	while ((next = strtok(NULL, ";")) != NULL)
	{
		Color color = get_color_from_sgf(next);
		if (color == 0) {
			fprintf(stderr, "sgf error : color\n");
			return;
		}

		XY xy = get_xy_from_sgf(next);

		board.move(xy, color, false);
	}
}

template <typename T>
void assert(const T& val, const T& expected)
{
	if (val == expected)
	{
		printf("success\n");
	}
	else {
		printf("fail\n");
	}
}

template <typename T>
void assert_not(const T& val, const T& expected)
{
	if (val != expected)
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

void test_002()
{
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

void test_003()
{
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

void test_004()
{
	// �A�^�����瓦����P�[�X

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

	// 9,7��NG
	assert_not(xy, get_xy(9, 7));
}

void test_005()
{
	// �����Ȃ̂ɏ����Ɣ���

	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		1, 1, 2, 2, 0, 1, 2, 2, 0, // 1
		1, 1, 2, 2, 2, 2, 0, 2, 2, // 2 
		1, 1, 1, 2, 0, 0, 0, 2, 1, // 3
		0, 1, 2, 2, 2, 0, 2, 2, 0, // 4
		1, 1, 1, 1, 2, 2, 1, 2, 0, // 5
		0, 0, 0, 1, 2, 1, 1, 2, 2, // 6
		0, 1, 1, 0, 1, 0, 1, 1, 2, // 7
		0, 1, 1, 0, 1, 0, 1, 2, 2, // 8
		0, 1, 1, 1, 1, 1, 1, 1, 2  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	XY xy = player.select_move(board, BLACK);

	print_child(player.root);
	printf("xy = %d, x,y = %d,%d\n", xy, get_x(xy), get_y(xy));

	assert(xy, PASS);
}

void test_ladder_search_001()
{
	// �V�`���E����
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 1
		0, 0, 0, 0, 2, 0, 0, 0, 0, // 2 
		0, 0, 0, 0, 2, 1, 2, 0, 0, // 3
		0, 0, 0, 0, 0, 2, 0, 0, 0, // 4
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 5
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 6
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 7
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
		0, 0, 0, 0, 0, 0, 0, 0, 0  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	Color tmp_board[BOARD_BYTE_MAX] = { 0 };
	XY liberties[4] = { get_xy(6, 1), get_xy(7, 2), 0, 0 };

	bool is_ladder = Board::ladder_search(board, BLACK, get_xy(6, 2), tmp_board, liberties, 5);

	assert(is_ladder, true);
}

void test_ladder_search_002()
{
	// �V�`���E����
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		0, 0, 0, 0, 0, 0, 1, 0, 0, // 1
		0, 0, 0, 0, 2, 0, 0, 0, 0, // 2 
		0, 0, 0, 0, 2, 1, 2, 0, 0, // 3
		0, 0, 0, 0, 0, 2, 0, 0, 0, // 4
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 5
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 6
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 7
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
		0, 0, 0, 0, 0, 0, 0, 0, 0  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	Color tmp_board[BOARD_BYTE_MAX] = { 0 };
	XY liberties[4] = { get_xy(6, 1), get_xy(7, 2), 0, 0 };

	bool is_ladder = Board::ladder_search(board, BLACK, get_xy(6, 2), tmp_board, liberties, 5);

	assert(is_ladder, true);
}

void test_ladder_search_003()
{
	// �V�`���E����
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		0, 0, 0, 0, 0, 0, 1, 1, 0, // 1
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 2 
		0, 0, 0, 2, 0, 0, 0, 0, 0, // 3
		0, 0, 0, 2, 1, 2, 0, 0, 0, // 4
		0, 0, 0, 0, 2, 0, 0, 0, 0, // 5
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 6
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 7
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
		0, 0, 0, 0, 0, 0, 0, 0, 0  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	Color tmp_board[BOARD_BYTE_MAX] = { 0 };
	XY liberties[4] = { get_xy(5, 2), get_xy(6, 3), 0, 0 };

	bool is_ladder = Board::ladder_search(board, BLACK, get_xy(5, 3), tmp_board, liberties, 5);

	assert(is_ladder, false);
}

void test_ladder_search_004()
{
	// 6,2�ɔ���u������ɗ�O����������

	// �V�`���E����
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		0, 0, 0, 0, 1, 0, 0, 0, 0, // 1
		0, 0, 0, 0, 0, 2, 0, 0, 0, // 2 
		0, 0, 0, 0, 0, 1, 2, 0, 0, // 3
		0, 1, 1, 0, 2, 1, 1, 2, 0, // 4
		0, 0, 0, 0, 1, 2, 2, 0, 0, // 5
		0, 0, 0, 0, 1, 0, 2, 0, 0, // 6
		0, 0, 0, 2, 1, 2, 1, 1, 2, // 7
		0, 0, 0, 0, 2, 1, 2, 2, 0, // 8
		0, 0, 0, 0, 0, 1, 0, 0, 0  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	XY xy = player.select_move(board, BLACK);

	print_child(player.root);
	printf("xy = %d, x,y = %d,%d\n", xy, get_x(xy), get_y(xy));

	assert(xy, get_xy(5, 3));
}

void test_ladder_search_005()
{
	// 7,7�ɔ���u������ɗ�O����������

	// �V�`���E����
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 1
		0, 1, 0, 0, 0, 0, 0, 0, 0, // 2 
		0, 0, 0, 0, 1, 0, 1, 0, 0, // 3
		0, 0, 0, 2, 1, 0, 0, 0, 0, // 4
		0, 0, 0, 0, 2, 0, 0, 0, 0, // 5
		0, 0, 0, 0, 2, 1, 2, 0, 0, // 6
		0, 0, 0, 2, 2, 1, 2, 0, 0, // 7
		0, 0, 0, 0, 0, 2, 0, 0, 0, // 8
		0, 0, 0, 0, 0, 1, 0, 0, 0  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	XY xy = player.select_move(board, BLACK);

	print_child(player.root);
	printf("xy = %d, x,y = %d,%d\n", xy, get_x(xy), get_y(xy));

	assert(xy, get_xy(6, 5));
}

void test_ladder_search_006()
{
	// 5,4�����������A7,6�ɒu���ꂽ

	// �V�`���E����
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		0, 0, 0, 0, 0, 1, 0, 0, 0, // 1
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 2 
		0, 0, 0, 2, 0, 0, 0, 0, 0, // 3
		0, 0, 2, 1, 0, 1, 0, 0, 0, // 4
		0, 2, 1, 1, 2, 0, 0, 0, 0, // 5
		0, 2, 1, 2, 0, 0, 0, 0, 0, // 6
		0, 0, 2, 0, 0, 0, 0, 0, 0, // 7
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
		0, 0, 0, 0, 1, 0, 0, 0, 0  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	XY xy = player.select_move(board, BLACK);

	print_child(player.root);
	printf("xy = %d, x,y = %d,%d\n", xy, get_x(xy), get_y(xy));

	assert(xy, get_xy(5, 4));
}

void test_ladder_search_007()
{
	// 6,9�����������A3,9�ɒu���ꂽ

	// �V�`���E����
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		0, 0, 0, 0, 0, 1, 0, 0, 0, // 1
		0, 0, 0, 0, 2, 0, 0, 0, 0, // 2 
		0, 0, 2, 1, 1, 2, 0, 0, 1, // 3
		0, 0, 0, 2, 1, 1, 2, 0, 0, // 4
		0, 0, 0, 0, 2, 1, 1, 2, 0, // 5
		0, 0, 0, 0, 2, 2, 1, 1, 2, // 6
		0, 0, 0, 2, 1, 1, 1, 1, 2, // 7
		0, 0, 1, 2, 1, 2, 1, 2, 0, // 8
		0, 0, 0, 1, 2, 0, 2, 0, 0  // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	XY xy = player.select_move(board, BLACK);

	print_child(player.root);
	printf("xy = %d, x,y = %d,%d\n", xy, get_x(xy), get_y(xy));

	assert(xy, get_xy(6, 9));
}

void test_is_self_atari_001()
{
	// �A�^���ɂȂ��
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 1
		1, 0, 0, 0, 0, 0, 0, 0, 0, // 2
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 3
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 4
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 5
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 6
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 7
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	bool res = board.is_self_atari(WHITE, get_xy(1, 1));

	assert(res, true);
}

void test_is_self_atari_002()
{
	// �A�^���ɂȂ�Ȃ���
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		1, 1, 0, 0, 0, 0, 0, 0, 0, // 1
		1, 0, 0, 0, 0, 0, 0, 0, 0, // 2
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 3
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 4
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 5
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 6
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 7
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	bool res = board.is_self_atari(WHITE, get_xy(2, 2));

	assert(res, false);
}

void test_is_self_atari_003()
{
	// �A�^���ɂȂ�Ȃ���(��邱�Ƃ��ł���)
	Color test_board[] = {
	//  1  2  3  4  5  6  7  8  9
		1, 1, 2, 2, 0, 0, 0, 0, 0, // 1
		1, 0, 1, 1, 2, 0, 0, 0, 0, // 2
		1, 0, 2, 2, 0, 0, 0, 0, 0, // 3
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 4
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 5
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 6
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 7
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	bool res = board.is_self_atari(WHITE, get_xy(2, 2));

	assert(res, false);
}

void test_is_self_atari_004()
{
	// �A�^���ɂȂ�Ȃ���(�R�E�ɂȂ�)
	Color test_board[] = {
		//  1  2  3  4  5  6  7  8  9
		1, 1, 2, 2, 0, 0, 0, 0, 0, // 1
		1, 0, 1, 2, 0, 0, 0, 0, 0, // 2
		1, 1, 2, 2, 0, 0, 0, 0, 0, // 3
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 4
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 5
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 6
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 7
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	bool res = board.is_self_atari(WHITE, get_xy(2, 2));

	assert(res, false);
}

void test_is_self_atari_005()
{
	// �A�^���ɂȂ��(�R�E�ɂȂ�Ȃ�)
	Color test_board[] = {
		//  1  2  3  4  5  6  7  8  9
		1, 1, 2, 2, 0, 0, 0, 0, 0, // 1
		1, 0, 1, 1, 2, 0, 0, 0, 0, // 2
		1, 1, 2, 2, 0, 0, 0, 0, 0, // 3
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 4
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 5
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 6
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 7
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
		0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
	};
	Board board(9);
	init_board(board, test_board, 9);
	debug_print_board(board);

	bool res = board.is_self_atari(WHITE, get_xy(2, 2));

	assert(res, true);
}

int main()
{
	//load_weight("../learn/rollout.bin");

	//test_001();
	//test_002();
	//test_003();
	//test_004();
	//test_005();
	//test_ladder_search_001();
	//test_ladder_search_002();
	//test_ladder_search_003();
	//test_ladder_search_004();
	//test_ladder_search_005();
	//test_ladder_search_006();
	//test_ladder_search_007();
	test_is_self_atari_001();
	test_is_self_atari_002();
	test_is_self_atari_003();
	test_is_self_atari_004();
	test_is_self_atari_005();
	return 0;
}