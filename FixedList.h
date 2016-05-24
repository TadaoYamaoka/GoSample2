#pragma once

#include "BitBoard.h"

template <typename T, typename I, const size_t size>
class FixedList
{
public:
	enum { END = (I)-1 };

	struct Elem {
		T val;
		I next;
		I prev;
	};

private:
	Elem list[size];
	BitBoard<size> unusedflg;
	I first;
	I last;

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

	I begin() const {
		return first;
	}

	I next(const I idx) const {
		return list[idx].next;
	}

	I end() const {
		return END;
	}

	I add() {
		// ���g�p�̃C���f�b�N�X��T��
		I idx = unusedflg.get_first_pos();

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

	void remove(const I idx) {
		// �C���f�b�N�X�𖢎g�p�ɂ���
		unusedflg.bit_test_and_set(idx);

		// �����N���X�g����
		I next = list[idx].next;
		I prev = list[idx].prev;
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

	T& operator[] (const I idx) {
		return list[idx].val;
	}

	const T& operator[] (const I idx) const {
		return list[idx].val;
	}
};
