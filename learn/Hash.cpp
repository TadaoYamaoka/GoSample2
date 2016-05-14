#include "Hash.h"
#include "../Random.h"

// パターン用ハッシュテーブル 12points
HashKey hash_key_pattern[HASH_KEY_MAX_PATTERN][256];
HashKey hash_key_move_pos[5 * 5];

// 各色のパターン用ハッシュキー値生成
void init_hash_table_and_weight(const uint64_t seed)
{
	// ハッシュテーブル初期化
	Random random(seed);

	for (int i = 0; i < HASH_KEY_MAX_PATTERN; i++)
	{
		for (int j = 0; j < 256; j++)
		{
			hash_key_pattern[i][j] = random.random() & HASH_KEY_MASK;
		}
	}

	for (int i = 0; i < sizeof(hash_key_move_pos) / sizeof(hash_key_move_pos[0]); i++)
	{
		hash_key_move_pos[i] = random.random() & HASH_KEY_MASK;
	}
}

