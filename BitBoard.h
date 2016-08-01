#pragma once

#ifndef _WIN64
typedef unsigned long BitBoardPart;
inline unsigned char bit_test(const BitBoardPart *a, long b)
{
	return _bittest((const long *)a, b);
}
inline unsigned char bit_test_and_set(BitBoardPart *a, long b)
{
	return _bittestandset((long *)a, b);
}
inline unsigned char bit_test_and_reset(BitBoardPart *a, long b)
{
	return _bittestandreset((long *)a, b);
}
inline unsigned char bit_scan_forward(unsigned long *Index, BitBoardPart Mask)
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

template <const int N>
class BitBoard
{
	BitBoardPart bitboard[(N + BIT - 1) / BIT];
public:
	int get_part_size() const {
		return (N + BIT - 1) / BIT;
	}

	const BitBoardPart get_bitboard_part(const int n) const {
		return bitboard[n];
	}

	BitBoardPart& get_bitboard_part(const int n) {
		return bitboard[n];
	}

	void set_bitboard_part(const int i, BitBoardPart val) {
		bitboard[i] = val;
	}

	void set_all_zero() {
		memset(bitboard, 0, sizeof(bitboard));
	}

	unsigned char bit_test(const short i) const {
		return ::bit_test(&bitboard[i / BIT], i % BIT);
	}

	unsigned char bit_test_and_set(const short i) {
		return ::bit_test_and_set(&bitboard[i / BIT], i % BIT);
	}

	unsigned char bit_test_and_reset(const short i) {
		return ::bit_test_and_reset(&bitboard[i / BIT], i % BIT);
	}

	unsigned char bit_test_and_reset(const int n, const short i) {
		return ::bit_test_and_reset(&bitboard[n], i);
	}

	unsigned char bit_scan_forward(const int n, unsigned long *Index) {
		return ::bit_scan_forward(Index, bitboard[n]);
	}

	void bit_reset_for_bsf(const int n) {
		bitboard[n] &= bitboard[n] - 1;
	}

	int merge_with_check(const BitBoard& src) {
		int cnt = 0;
		for (int i = 0; i < sizeof(bitboard) / sizeof(bitboard[0]); i++)
		{
			// 重複している数をカウント
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

	short get_first_pos() const {
		short pos = 0;
		for (int i = 0; i < sizeof(bitboard) / sizeof(bitboard[0]); i++, pos += BIT)
		{
			unsigned long idx;
			if (::bit_scan_forward(&idx, bitboard[i]))
			{
				return pos + (short)idx;
			}
		}
		return -1;
	}

	int get_four_pos(short pos[4]) const {
		int num = 0;
		short pos_tmp = 0;
		for (int i = 0; i < sizeof(bitboard) / sizeof(bitboard[0]) && num < 4; i++, pos_tmp += BIT)
		{
			BitBoardPart bitboard_tmp = bitboard[i];
			unsigned long idx;
			while (::bit_scan_forward(&idx, bitboard_tmp))
			{
				pos[num++] = pos_tmp + (short)idx;
				if (num == 4)
				{
					return num;
				}
				::bit_test_and_reset(&bitboard_tmp, idx);
			}
		}
		return num;
	}
};
