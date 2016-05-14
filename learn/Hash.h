#pragma once

#include "Pattern.h"

typedef unsigned int HashKey;

const int HASH_KEY_MAX_PATTERN = 12 / 2; // color,libertiesのセット4byte * 2単位
const int HASH_KEY_BIT = 24;
const int HASH_KEY_MAX = 1 << HASH_KEY_BIT;
const int HASH_KEY_MASK = HASH_KEY_MAX - 1;

extern HashKey hash_key_pattern[HASH_KEY_MAX_PATTERN][256];
extern HashKey hash_key_move_pos[5 * 5];

extern void init_hash_table_and_weight(const uint64_t seed);

// レスポンスパターン用ハッシュキー値取得
inline HashKey get_hash_key_response_pattern(const ResponsePatternVal& val)
{
	return hash_key_pattern[0][val.vals.color_liberties[0]]
		^ hash_key_pattern[1][val.vals.color_liberties[1]]
		^ hash_key_pattern[2][val.vals.color_liberties[2]]
		^ hash_key_pattern[3][val.vals.color_liberties[3]]
		^ hash_key_pattern[4][val.vals.color_liberties[4]]
		^ hash_key_pattern[5][val.vals.color_liberties[5]]
		^ hash_key_move_pos[val.vals.move_pos];
}

// ノンレスポンスパターン用ハッシュキー値取得
inline HashKey get_hash_key_nonresponse_pattern(const NonResponsePatternVal& val)
{
	return hash_key_pattern[0][val.vals.color_liberties[0]]
		^ hash_key_pattern[1][val.vals.color_liberties[1]]
		^ hash_key_pattern[2][val.vals.color_liberties[2]]
		^ hash_key_pattern[3][val.vals.color_liberties[3]];
}

// 12point diamondパターン用ハッシュキー値取得
inline HashKey get_hash_key_diamond12_pattern(const Diamond12PatternVal& val)
{
	return hash_key_pattern[0][val.vals.color_liberties[0]]
		^ hash_key_pattern[1][val.vals.color_liberties[1]]
		^ hash_key_pattern[2][val.vals.color_liberties[2]]
		^ hash_key_pattern[3][val.vals.color_liberties[3]]
		^ hash_key_pattern[4][val.vals.color_liberties[4]]
		^ hash_key_pattern[5][val.vals.color_liberties[5]];
}

// rollout policyの重み
struct RolloutPolicyWeightHash
{
	// アタリを防ぐ手の重み
	float save_atari_weight;

	// 直前の手と隣接
	float neighbour_weight;

	// レスポンスマッチの重み
	float response_match_weight;

	// レスポンスパターンの重み
	float response_pattern_weight[HASH_KEY_MAX];

	// ノンレスポンスパターンの重み
	float nonresponse_pattern_weight[HASH_KEY_MAX];
};

// tree policyの重み
struct TreePolicyWeightHash : public RolloutPolicyWeightHash
{
	// アタリになる手
	float self_atari_weight;

	// 2手前からの距離
	float last_move_distance_weight[2][17];

	// ノンレスポンスパターン(12-point diamond)
	float diamond12_pattern_weight[HASH_KEY_MAX];
};
