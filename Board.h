#pragma once

#include <intrin.h>
#include <memory.h>
#include "BitBoard.h"
#include "FixedList.h"

extern int BOARD_SIZE;
extern int BOARD_WIDTH;
extern int BOARD_STONE_MAX;
extern int BOARD_MAX;
const int BOARD_SIZE_MAX = 19;
const int GROUP_SIZE_MAX = BOARD_SIZE_MAX * BOARD_SIZE_MAX / 2;
const int BOARD_BYTE_MAX = (BOARD_SIZE_MAX + 1) * (BOARD_SIZE_MAX + 2) + 1;

extern double KOMI;

typedef short XY;

const XY PASS = 0;

extern XY DIR4[4];

typedef unsigned char Color;
const Color EMPTY = 0;
const Color BLACK = 1;
const Color WHITE = 2;
const Color OFFBOARD = 3;

typedef unsigned char GroupIndex;
const GroupIndex G_NONE = 0xff;
const GroupIndex G_OFFBOARD = G_NONE - 1;

inline int get_x(const XY xy)
{
	return xy % BOARD_WIDTH;
}

inline int get_y(const XY xy)
{
	return xy / BOARD_WIDTH;
}

inline XY get_xy(const int x, const int y)
{
	return x + BOARD_WIDTH * y;
}

inline Color opponent(const Color color)
{
	return (Color)(((int)color) ^ 0x3);
}

enum MoveResult {
	SUCCESS,
	ILLIGAL,
	KO,
	EYE
};

class Group
{
public:
	// 連を構成する石の位置
	XY stone[BOARD_SIZE_MAX * BOARD_SIZE_MAX];

	// 呼吸点の位置(ビットボード)
	BitBoard<BOARD_BYTE_MAX> liberty_bitboard;

	Color color;
	int stone_num;
	int liberty_num;

	// 隣接する敵の連番号(ビットボード)
	BitBoard<GROUP_SIZE_MAX> adjacent;

	// 呼吸点が一致するか
	bool hit_liberties(const XY xy) const {
		return liberty_bitboard.bit_test(xy);
	}

	// 連に石と呼吸点を追加
	void add_stone_and_liberties(const XY xy, const XY around_liberty[4]) {
		// 石を追加
		stone[stone_num++] = xy;

		// 呼吸点の削除
		remove_liberty(xy);

		// 呼吸点の追加
		for (int i = 0; i < 4; i++)
		{
			if (around_liberty[i] == 0)
			{
				continue;
			}
			XY xyd = xy + DIR4[i];
			add_liberty(xyd);
		}
	}

	// 連の連結
	void chain_group(const XY xy, Group& group)
	{
		// 連結前に呼吸点を削除
		group.liberty_bitboard.bit_test_and_reset(xy);
		group.liberty_num--;

		// 石を追加
		memcpy(stone + stone_num, group.stone, group.stone_num * sizeof(stone[0]));
		stone_num += group.stone_num;

		// 呼吸点を追加
		liberty_num -= liberty_bitboard.merge_with_check(group.liberty_bitboard);
		liberty_num += group.liberty_num;

		// 隣接する敵の連番号をマージ
		adjacent.merge(group.adjacent);
	}

	// 呼吸点の追加
	void add_liberty(const XY xy)
	{
		if (liberty_bitboard.bit_test_and_set(xy))
		{
			// すでに呼吸点だったときは加算しない
		}
		else {
			liberty_num++;
		}
	}

	// 呼吸点の削除
	void remove_liberty(const XY xy)
	{
		liberty_bitboard.bit_test_and_reset(xy);
		liberty_num--;
	}

	// 一つ目の呼吸点を取得
	XY get_first_liberty() const {
		return liberty_bitboard.get_first_pos();
	}

	// 呼吸点を4つまで取得
	int get_four_liberties(XY pos[4]) const {
		return liberty_bitboard.get_four_pos(pos);
	}
};

class Board
{
public:
	// 盤面(連番号を保持)
	GroupIndex board[BOARD_BYTE_MAX];

	// 連
	FixedList<Group, GROUP_SIZE_MAX> groups;

	XY ko;
	XY pre_xy;

	// 石の数
	int stone_num[3];

	Board() {}
	Board(const int size) {
		init(size);
	}

	void init(const int size)
	{
		BOARD_SIZE = size;
		BOARD_WIDTH = BOARD_SIZE + 1;
		BOARD_STONE_MAX = BOARD_SIZE * BOARD_SIZE;
		BOARD_MAX = BOARD_WIDTH * (BOARD_SIZE + 2);
		memset(board, G_NONE, BOARD_MAX);
		memset(board, G_OFFBOARD, BOARD_WIDTH);
		memset(board + BOARD_WIDTH * (BOARD_SIZE + 1), G_OFFBOARD, BOARD_WIDTH);
		stone_num[BLACK] = stone_num[WHITE] = 0;
		for (int y = 1; y <= BOARD_SIZE; y++)
		{
			board[BOARD_WIDTH * y] = G_OFFBOARD;
		}
		board[BOARD_WIDTH * (BOARD_SIZE + 2)] = G_OFFBOARD;
		groups.init();
		DIR4[0] = -1;
		DIR4[1] = 1;
		DIR4[2] = -BOARD_WIDTH;
		DIR4[3] = BOARD_WIDTH;
		ko = -1;
		pre_xy = -1;
	}

	const Color operator[](const XY xy) const {
		if (board[xy] == G_NONE)
		{
			return EMPTY;
		}
		else if (board[xy] == G_OFFBOARD)
		{
			return OFFBOARD;
		}
		return groups[board[xy]].color;
	}

	bool is_empty(const XY xy) const {
		return board[xy] == G_NONE;
	}

	bool is_offboard(const XY xy) const {
		return board[xy] == G_OFFBOARD;
	}

	MoveResult move(const XY xy, const Color color, const bool fill_eye_err = true);
	MoveResult is_legal(const XY xy, const Color color, const bool fill_eye_err = true) const;
	void move_legal(const XY xy, const Color color);

	GroupIndex add_group(const XY xy, const Color color, const XY around_liberty[4]) {
		GroupIndex last = groups.add();

		Group& group = groups[last];
		group.color = color;
		group.stone_num = 1;
		group.stone[0] = xy;
		group.liberty_num = 0;
		group.liberty_bitboard.set_all_zero();
		// 呼吸点
		for (int i = 0; i < 4; i++)
		{
			if (around_liberty[i] != 0)
			{
				XY xyd = xy + DIR4[i];
				group.liberty_bitboard.bit_test_and_set(xyd);
				group.liberty_num++;
			}
		}
		group.adjacent.set_all_zero();
		return last;
	}

	void remove_group(const size_t idx) {
		groups.remove(idx);
	}

	// グループ取得
	const Group& get_group(const XY xy) const {
		return groups[board[xy]];
	}

};

