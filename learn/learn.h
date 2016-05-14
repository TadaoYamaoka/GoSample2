#pragma once

#include "Pattern.h"
#include "Hash.h"

extern void prepare_pattern(const wchar_t* dirs);
extern void learn_pattern(const wchar_t* dirs, const int game_num, const int iteration_num, const float eta, const float ramda);
extern void check_hash();
extern void dump_weight();
extern void print_response_pattern(const ResponsePatternVal& val);
extern void print_nonresponse_pattern(const NonResponsePatternVal& val);
extern void print_diamond12_pattern(const Diamond12PatternVal& val);
extern void clean_kifu(const wchar_t* dirs);
extern void init_weight();
extern void print_pattern(const wchar_t* kind, const wchar_t* val);
extern void sort_weight();

// ハッシュキー衝突検出用
extern ResponsePatternVal response_pattern_collision[HASH_KEY_MAX];
extern NonResponsePatternVal nonresponse_pattern_collision[HASH_KEY_MAX];
extern Diamond12PatternVal diamond12_pattern_collision[HASH_KEY_MAX];
