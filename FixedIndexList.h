#pragma once

template<typename I, const size_t size>
class FixedIndexList
{
private:
	enum { END = (I)-1 };
	I nextlist[size];
	I prevlist[size];
	I first;
	I last;

public:
	void init() {
		first = END;
		last = END;
	}

	I begin() const {
		return first;
	}

	I next(const I idx) const {
		return nextlist[idx];
	}

	I end() const {
		return END;
	}

	I add(const I idx) {
		// リンクリスト結合
		if (first == END)
		{
			first = idx;
			prevlist[idx] = END;
		}
		else {
			nextlist[last] = idx;
			prevlist[idx] = last;
		}
		nextlist[idx] = END;
		last = idx;

		return last;
	}

	void remove(const I idx) {
		// リンクリスト解除
		if (idx == first)
		{
			first = nextlist[idx];
			prevlist[first] = END;
		}
		else if (idx == last)
		{
			last = prevlist[idx];
			nextlist[last] = END;
		}
		else
		{
			nextlist[prevlist[idx]] = nextlist[idx];
			prevlist[nextlist[idx]] = prevlist[idx];
		}
	}
};
