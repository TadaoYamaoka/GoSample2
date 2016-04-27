#pragma once

#include "BitBoard.h"

template <typename T, const size_t size>
class FixedList
{
public:
	enum { END = 255 };

	struct Elem {
		T val;
		unsigned char next;
		unsigned char prev;
	};

private:
	Elem list[size];
	BitBoard<size> unusedflg;
	unsigned char first;
	unsigned char last;

public:
	void init() {
		first = END;
		last = END;
		for (int i = 0; i < unusedflg.get_part_size() - 1; i++)
		{
			unusedflg.set_bitboard_part(i, -1);
		}
		unusedflg.set_bitboard_part(unusedflg.get_part_size() - 1, (1ll << (size % BIT)) - 1);
	}

	unsigned char begin() const {
		return first;
	}

	unsigned char next(const unsigned char idx) const {
		return list[idx].next;
	}

	unsigned char end() const {
		return END;
	}

	unsigned char add() {
		// ���g�p�̃C���f�b�N�X��T��
		unsigned char idx = unusedflg.get_first_pos();

		// �g�p���ɂ���
		unusedflg.bit_test_and_reset(idx);

		// �����N���X�g����
		if (first == END)
		{
			first = idx;
			list[idx].prev = END;
		}
		else {
			list[last].next = idx;
			list[idx].prev = last;
		}
		list[idx].next = END;
		last = idx;

		return last;
	}

	void remove(const unsigned char idx) {
		// �C���f�b�N�X�𖢎g�p�ɂ���
		unusedflg.bit_test_and_set(idx);

		// �����N���X�g����
		unsigned char next = list[idx].next;
		unsigned char prev = list[idx].prev;
		if (idx == first)
		{
			first = next;
			list[first].prev = END;
		}
		else if (idx == last)
		{
			last = prev;
			list[last].next = END;
		}
		else
		{
			list[prev].next = next;
			list[next].prev = prev;
		}
	}

	T& operator[] (const unsigned char idx) {
		return list[idx].val;
	}

	const T& operator[] (const unsigned char idx) const {
		return list[idx].val;
	}
};
