#pragma once

#ifndef _WIN64
#include <stdlib.h>
const int RANDOM_MAX = RAND_MAX;
inline void srandom(unsigned int seed)
{
	srand(seed);
}
inline int random()
{
	return rand();
}
#else
#include <stdint.h>

extern uint64_t xorshift64star_x; // The state must be seeded with a nonzero value.
const uint64_t RANDOM_MAX = -1; // 2^64 - 1

inline void srandom(uint64_t seed)
{
	xorshift64star_x = seed;
}
inline uint64_t random(void) {
	xorshift64star_x ^= xorshift64star_x >> 12; // a
	xorshift64star_x ^= xorshift64star_x << 25; // b
	xorshift64star_x ^= xorshift64star_x >> 27; // c
	return xorshift64star_x * UINT64_C(2685821657736338717);
}
#endif
