#pragma once
#include <chrono>

#ifndef _WIN64
#include <stdlib.h>
const int RANDOM_MAX = RAND_MAX;
class Random
{
public:
	Random() {
		srand(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	int random()
	{
		return rand();
	}
};
#else
#include <stdint.h>
const uint64_t RANDOM_MAX = -1; // 2^64 - 1

class Random
{
	uint64_t xorshift64star_x; // The state must be seeded with a nonzero value.
public:
	Random() {
		xorshift64star_x = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	uint64_t random() {
		xorshift64star_x ^= xorshift64star_x >> 12; // a
		xorshift64star_x ^= xorshift64star_x << 25; // b
		xorshift64star_x ^= xorshift64star_x >> 27; // c
		return xorshift64star_x * UINT64_C(2685821657736338717);
	}
};
#endif
