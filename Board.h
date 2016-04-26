#pragma once

#include <intrin.h>
#include <memory.h>

extern int BOARD_SIZE;
extern int BOARD_WIDTH;
extern int BOARD_STONE_MAX;
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
typedef unsigned long long BitBoardPart;
inline unsigned char bit_test(const BitBoardPart *a, long long b)
{
	return _bittest64((const long long *)a, b);
}
inline unsigned char bit_test_and_set(BitBoardPart *a, long long b)
{
	return _bittestandset64((long long *)a, b);
}
inline unsigned char bit_test_and_reset(BitBoardPart *a, long long b)
{
	return _bittestandreset64((long long *)a, b);
}
inline unsigned char bit_scan_forward(unsigned long *Index, BitBoardPart Mask)
{
	return _BitScanForward64(Index, Mask);
}
inline int popcnt(unsigned long long value)
{
	return (int)__popcnt64(value);
}
#endif
const int BIT = sizeof(BitBoardPart) * 8;

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

template <const int N>
class BitBoard
{
	BitBoardPart bitboard[(N + BIT - 1) / BIT];
public:
	int get_part_size() const {
		return (N + BIT - 1) / BIT;
	}

	BitBoardPart get_bitboard_part(const int n) const {
		return bitboard[n];
	}

	void set_bitboard_part(const int i, BitBoardPart val) {
		bitboard[i] = val;
	}

	void set_all_zero() {
		memset(bitboard, 0, sizeof(bitboard));
	}

	unsigned char bit_test(const int i) const {
		return ::bit_test(&bitboard[i / BIT], i % BIT);
	}

	unsigned char bit_test_and_set(const int i) {
		return ::bit_test_and_set(&bitboard[i / BIT], i % BIT);
	}

	unsigned char bit_test_and_reset(const int i) {
		return ::bit_test_and_reset(&bitboard[i / BIT], i % BIT);
	}

	unsigned char bit_test_and_reset(const int n, const int i) {
		return ::bit_test_and_reset(&bitboard[n], i);
	}

	unsigned char bit_scan_forward(const int n, unsigned long *Index) {
		return ::bit_scan_forward(Index, bitboard[n]);
	}

	int merge_with_check(const BitBoard& src) {
		int cnt = 0;
		for (int i = 0; i < sizeof(bitboard) / sizeof(bitboard[0]); i++)
		{
			// �d�����Ă��鐔���J�E���g
			cnt += popcnt(bitboard[i] & src.bitboard[i]);

			bitboard[i] |= src.bitboard[i];
		}
		return cnt;
	}

	void merge(const BitBoard& src) {
		for (int i = 0; i < sizeof(bitboard) / sizeof(bitboard[0]); i++)
		{
			bitboard[i] |= src.bitboard[i];
		}
	}

	int get_first_pos() const {
		int pos = 0;
		for (int i = 0; i < sizeof(bitboard) / sizeof(bitboard[0]); i++, pos += BIT)
		{
			unsigned long idx;
			if (::bit_scan_forward(&idx, bitboard[i]))
			{
				return pos + idx;
			}
		}
		return -1;
	}
};

class Group
{
public:
	// �A���\������΂̈ʒu
	XY stone[BOARD_SIZE_MAX * BOARD_SIZE_MAX];

	// �ċz�_�̈ʒu(�r�b�g�{�[�h)
	BitBoard<BOARD_BYTE_MAX> liberty_bitboard;

	Color color;
	int stone_num;
	int liberty_num;

	// �אڂ���G�̘A�ԍ�(�r�b�g�{�[�h)
	BitBoard<GROUP_SIZE_MAX> adjacent;

	// �ċz�_����v���邩
	bool hit_liberties(const XY xy) const {
		return liberty_bitboard.bit_test(xy);
	}

	// �A�ɐ΂ƌċz�_��ǉ�
	void add_stone_and_liberties(const XY xy, const XY around_liberty[4]) {
		// �΂�ǉ�
		stone[stone_num++] = xy;

		// �ċz�_�̍폜
		remove_liberty(xy);

		// �ċz�_�̒ǉ�
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

	// �A�̘A��
	void chain_group(const XY xy, Group& group)
	{
		// �A���O�Ɍċz�_���폜
		group.liberty_bitboard.bit_test_and_reset(xy);
		group.liberty_num--;

		// �΂�ǉ�
		memcpy(stone + stone_num, group.stone, group.stone_num * sizeof(stone[0]));
		stone_num += group.stone_num;

		// �ċz�_��ǉ�
		liberty_num -= liberty_bitboard.merge_with_check(group.liberty_bitboard);
		liberty_num += group.liberty_num;

		// �אڂ���G�̘A�ԍ����}�[�W
		adjacent.merge(group.adjacent);
	}

	// �ċz�_�̒ǉ�
	void add_liberty(const XY xy)
	{
		if (liberty_bitboard.bit_test_and_set(xy))
		{
			// ���łɌċz�_�������Ƃ��͉��Z���Ȃ�
		}
		else {
			liberty_num++;
		}
	}

	// �ċz�_�̍폜
	void remove_liberty(const XY xy)
	{
		liberty_bitboard.bit_test_and_reset(xy);
		liberty_num--;
	}

	// ��ڂ̌ċz�_�̏ꍇ
	XY get_first_liberty() const {
		return liberty_bitboard.get_first_pos();
	}
};

class Board
{
public:
	// �Ֆ�(�A�ԍ���ێ�)
	GroupIndex board[BOARD_BYTE_MAX];

	// �A
	Group group[GROUP_SIZE_MAX];
	int group_num;
	BitBoard<GROUP_SIZE_MAX> group_unusedflg;

	XY ko;
	XY pre_xy;

	// �΂̐�
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
		group_num = 0;
		for (int i = 0; i < group_unusedflg.get_part_size() - 1; i++)
		{
			group_unusedflg.set_bitboard_part(i, -1);
		}
		group_unusedflg.set_bitboard_part(group_unusedflg.get_part_size() - 1, (1ll << (GROUP_SIZE_MAX % BIT)) - 1);

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
		return group[board[xy]].color;
	}

	bool is_empty(const XY xy) const {
		return board[xy] == G_NONE;
	}

	bool is_offboard(const XY xy) const {
		return board[xy] == G_OFFBOARD;
	}

	MoveResult move(const XY xy, const Color color, const bool fill_eye_err = true);
	MoveResult is_legal(const XY xy, const Color color, const bool fill_eye_err = true) const;

	int add_group(const XY xy, const Color color, const XY around_liberty[4]) {
		// ���g�p�̃C���f�b�N�X��T��
		int idx = group_unusedflg.get_first_pos();

		// �g�p���ɂ���
		group_unusedflg.bit_test_and_reset(idx);

		Group& last = group[idx];

		group_num++;
		last.color = color;
		last.stone_num = 1;
		last.stone[0] = xy;
		last.liberty_num = 0;
		last.liberty_bitboard.set_all_zero();
		// �ċz�_
		for (int i = 0; i < 4; i++)
		{
			if (around_liberty[i] != 0)
			{
				XY xyd = xy + DIR4[i];
				last.liberty_bitboard.bit_test_and_set(xyd);
				last.liberty_num++;
			}
		}
		last.adjacent.set_all_zero();
		return idx;
	}

	void remove_group(const size_t idx) {
		group_num--;

		// �C���f�b�N�X�𖢎g�p�ɂ���
		group_unusedflg.bit_test_and_set(idx);
	}

	// �O���[�v�擾
	const Group* get_group(const XY xy) const {
		return &group[board[xy]];
	}

};

