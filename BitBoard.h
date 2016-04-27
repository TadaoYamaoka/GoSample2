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

	BitBoardPart get_bitboard_part(const int n) const {
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