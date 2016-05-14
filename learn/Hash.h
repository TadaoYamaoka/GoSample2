#pragma once

#include "Pattern.h"

typedef unsigned int HashKey;

const int HASH_KEY_MAX_PATTERN = 12 / 2; // color,liberties�̃Z�b�g4byte * 2�P��
const int HASH_KEY_BIT = 24;
const int HASH_KEY_MAX = 1 << HASH_KEY_BIT;
const int HASH_KEY_MASK = HASH_KEY_MAX - 1;

extern HashKey hash_key_pattern[HASH_KEY_MAX_PATTERN][256];
extern HashKey hash_key_move_pos[5 * 5];

extern void init_hash_table_and_weight(const uint64_t seed);

// ���X�|���X�p�^�[���p�n�b�V���L�[�l�擾
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

// �m�����X�|���X�p�^�[���p�n�b�V���L�[�l�擾
inline HashKey get_hash_key_nonresponse_pattern(const NonResponsePatternVal& val)
{
	return hash_key_pattern[0][val.vals.color_liberties[0]]
		^ hash_key_pattern[1][val.vals.color_liberties[1]]
		^ hash_key_pattern[2][val.vals.color_liberties[2]]
		^ hash_key_pattern[3][val.vals.color_liberties[3]];
}

// 12point diamond�p�^�[���p�n�b�V���L�[�l�擾
inline HashKey get_hash_key_diamond12_pattern(const Diamond12PatternVal& val)
{
	return hash_key_pattern[0][val.vals.color_liberties[0]]
		^ hash_key_pattern[1][val.vals.color_liberties[1]]
		^ hash_key_pattern[2][val.vals.color_liberties[2]]
		^ hash_key_pattern[3][val.vals.color_liberties[3]]
		^ hash_key_pattern[4][val.vals.color_liberties[4]]
		^ hash_key_pattern[5][val.vals.color_liberties[5]];
}

// rollout policy�̏d��
struct RolloutPolicyWeightHash
{
	// �A�^����h����̏d��
	float save_atari_weight;

	// ���O�̎�Ɨא�
	float neighbour_weight;

	// ���X�|���X�}�b�`�̏d��
	float response_match_weight;

	// ���X�|���X�p�^�[���̏d��
	float response_pattern_weight[HASH_KEY_MAX];

	// �m�����X�|���X�p�^�[���̏d��
	float nonresponse_pattern_weight[HASH_KEY_MAX];
};

// tree policy�̏d��
struct TreePolicyWeightHash : public RolloutPolicyWeightHash
{
	// �A�^���ɂȂ��
	float self_atari_weight;

	// 2��O����̋���
	float last_move_distance_weight[2][17];

	// �m�����X�|���X�p�^�[��(12-point diamond)
	float diamond12_pattern_weight[HASH_KEY_MAX];
};
