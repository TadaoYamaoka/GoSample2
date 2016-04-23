#pragma once

#include <intrin.h>
#include <memory.h>

extern int BOARD_SIZE;
extern int BOARD_WIDTH;
extern int BOARD_MAX;
const int BOARD_SIZE_MAX = 19;
const int GROUP_SIZE_MAX = BOARD_SIZE_MAX * BOARD_SIZE_MAX / 2;
const int BOARD_BYTE_MAX = (BOARD_SIZE_MAX + 1) * (BOARD_SIZE_MAX + 2);

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

#ifndef _WIN64
typedef unsigned long BitBoard;
inline unsigned char bit_test(const BitBoard *a, long b)
{
	return _bittest((const long *)a, b);
}
inline unsigned char bit_test_and_set(BitBoard *a, long b)
{
	return _bittestandset((long *)a, b);
}
inline unsigned char bit_test_and_reset(BitBoard *a, long b)
{
	return _bittestandreset((long *)a, b);
}
inline unsigned char bit_scan_forward(unsigned long *Index, BitBoard Mask)
{
	return _BitScanForward(Index, Mask);
}
inline int popcnt(unsigned long value)
{
	return (int)__popcnt(value);
}

#else
typedef unsigned long long BitBoard;
inline unsigned char bit_test(const BitBoard *a, long long b)
{
	return _bittest64((const long long *)a, b);
}
inline unsigned char bit_test_and_set(BitBoard *a, long long b)
{
	return _bittestandset64((long long *)a, b);
}
inline unsigned char bit_test_and_reset(BitBoard *a, long long b)
{
	return _bittestandreset64((long long *)a, b);
}
inline unsigned char bit_scan_forward(unsigned long *Index, BitBoard Mask)
{
	return _BitScanForward64(Index, Mask);
}
inline int popcnt(unsigned long long value)
{
	return (int)__popcnt64(value);
}
#endif
const int BIT = sizeof(BitBoard) * 8;

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
	BitBoard liberty_bitboard[BOARD_BYTE_MAX / BIT + 1];

	Color color;
	int stone_num;
	int liberty_num;

	// 連の一部か
	bool hit_stones(const XY xy) {
		return stone[xy] == color;
	}

	// 呼吸点が一致するか
	bool hit_liberties(const XY xy) {
		return bit_test(&liberty_bitboard[xy / BIT], xy % BIT);
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
		bit_test_and_reset(&group.liberty_bitboard[xy / BIT], xy % BIT);
		group.liberty_num--;

		// 石を追加
		memcpy(stone + stone_num, group.stone, group.stone_num * sizeof(stone[0]));
		stone_num += group.stone_num;

		// 呼吸点を追加
		for (int i = 0; i < sizeof(liberty_bitboard) / sizeof(liberty_bitboard[0]); i++)
		{
			// 重複チェック
			liberty_num -= popcnt(liberty_bitboard[i] & group.liberty_bitboard[i]);

			liberty_bitboard[i] |= group.liberty_bitboard[i];
		}
		liberty_num += group.liberty_num;
	}

	// 呼吸点の追加
	void add_liberty(const XY xy)
	{
		if (bit_test_and_set(&liberty_bitboard[xy / BIT], xy % BIT))
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
		bit_test_and_reset(&liberty_bitboard[xy / BIT], xy % BIT);
		liberty_num--;
	}
};

class Board
{
	// 盤面(連番号を保持)
	GroupIndex board[BOARD_BYTE_MAX];

	// 連
	Group group[GROUP_SIZE_MAX];
	int group_num;
	BitBoard group_unusedflg[GROUP_SIZE_MAX / BIT + 1];

public:
	XY ko;

	Board() {}
	Board(const int size) {
		init(size);
	}

	void init(const int size)
	{
		BOARD_SIZE = size;
		BOARD_WIDTH = BOARD_SIZE + 1;
		BOARD_MAX = BOARD_WIDTH * (BOARD_SIZE + 2);
		memset(board, G_NONE, BOARD_MAX);
		memset(board, G_OFFBOARD, BOARD_WIDTH);
		memset(board + BOARD_WIDTH * (BOARD_SIZE + 1), G_OFFBOARD, BOARD_WIDTH);
		for (int y = 1; y <= BOARD_SIZE; y++)
		{
			board[BOARD_WIDTH * y] = G_OFFBOARD;
		}
		group_num = 0;
		for (int i = 0; i < sizeof(group_unusedflg) / sizeof(group_unusedflg[0]) - 1; i++)
		{
			group_unusedflg[i] = -1;
		}
		group_unusedflg[sizeof(group_unusedflg) / sizeof(group_unusedflg[0]) - 1] = (1ll << (GROUP_SIZE_MAX % BIT)) - 1;

		DIR4[0] = -1;
		DIR4[1] = 1;
		DIR4[2] = -BOARD_WIDTH;
		DIR4[3] = BOARD_WIDTH;
		ko = -1;
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
		return group[board[xy]].color;
	}

	bool is_empty(const XY xy) const {
		return board[xy] == G_NONE;
	}

	MoveResult move(const XY xy, const Color color, const bool fill_eye_err = true);

	int add_group(const XY xy, const Color color, const XY around_liberty[4]) {
		// 未使用のインデックスを探す
		int idx = 0;
		for (int i = 0; i < sizeof(group_unusedflg) / sizeof(group_unusedflg[0]); i++, idx += BIT)
		{
			unsigned long idx_found;
			if (bit_scan_forward(&idx_found, group_unusedflg[i]))
			{
				idx += idx_found;
				// 使用中にする
				bit_test_and_reset(&group_unusedflg[i], idx_found);
				break;
			}
		}
		Group& last = group[idx];

		group_num++;
		last.color = color;
		last.stone_num = 1;
		last.stone[0] = xy;
		last.liberty_num = 0;
		memset(last.liberty_bitboard, 0, sizeof(last.liberty_bitboard));
		// 呼吸点
		for (int i = 0; i < 4; i++)
		{
			if (around_liberty[i] != 0)
			{
				XY xyd = xy + DIR4[i];
				bit_test_and_set(&last.liberty_bitboard[xyd / BIT], xyd % BIT);
				last.liberty_num++;
			}
		}
		return idx;
	}
	void remove_group(const size_t idx) {
		group_num--;

		// インデックスを未使用にする
		bit_test_and_set(&group_unusedflg[idx / BIT], idx % BIT);
	}
};

