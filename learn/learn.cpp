#include <windows.h>
#include <string>
#include <cassert>

#include "../Board.h"
#include "../Random.h"

using namespace std;

Random random;

typedef unsigned int PatternVal;
typedef unsigned int HashKey;

// 学習係数
float eta = 0.01;

// レスポンスパターン用ハッシュテーブル 12points
const int HASH_KEY_MAX_PATTERN_COLOR = 4096; // 2^12
const int HASH_KEY_MAX_PATTERN_LIBERTIES = 4096 * 4096;
HashKey hash_key_pattern_black[HASH_KEY_MAX_PATTERN_COLOR];
HashKey hash_key_pattern_white[HASH_KEY_MAX_PATTERN_COLOR];
HashKey hash_key_pattern_liberties[HASH_KEY_MAX_PATTERN_LIBERTIES];
HashKey hash_key_move_pos[5 * 5];

const unsigned int HASH_KEY_MAX = 0x20000000;

struct ResponsePatternVal
{
	PatternVal black;
	PatternVal white;
	PatternVal liberties;
	PatternVal move_pos;

	bool operator ==(const ResponsePatternVal& val) {
		return val.black == black && val.white == white && val.liberties == liberties && val.move_pos == move_pos;
	}

	bool operator !=(const ResponsePatternVal& val) {
		return !(*this == val);
	}
};

struct NonResponsePatternVal
{
	PatternVal black;
	PatternVal white;
	PatternVal liberties;

	bool operator ==(const NonResponsePatternVal& val) {
		return val.black == black && val.white == white && val.liberties == liberties;
	}

	bool operator !=(const NonResponsePatternVal& val) {
		return !(*this == val);
	}
};


// レスポンスマッチの重み
float response_match_weight;

// レスポンスパターンの重み
float* response_pattern_weight = new float[HASH_KEY_MAX];

// ノンレスポンスパターンの重み
float* nonresponse_pattern_weight = new float[HASH_KEY_MAX];

// ハッシュキー衝突検出用
ResponsePatternVal* response_pattern_collision = new ResponsePatternVal[HASH_KEY_MAX];
NonResponsePatternVal* nonresponse_pattern_collision = new NonResponsePatternVal[HASH_KEY_MAX];
ResponsePatternVal response_pattern_collision0 = { 0, 0, 0, 0 };
NonResponsePatternVal nonresponse_pattern_collision0 = { 0, 0, 0 };

// 各色のパターン用ハッシュキー値生成
void init_hash_table_and_weight() {
	// ハッシュテーブル初期化
	for (int i = 0; i < HASH_KEY_MAX_PATTERN_COLOR; i++)
	{
		hash_key_pattern_black[i] = random.random() % HASH_KEY_MAX;
		hash_key_pattern_white[i] = random.random() % HASH_KEY_MAX;
	}

	for (int i = 0; i < HASH_KEY_MAX_PATTERN_LIBERTIES; i++)
	{
		hash_key_pattern_liberties[i] = random.random() % HASH_KEY_MAX;
	}

	for (int i = 0; i < sizeof(hash_key_move_pos) / sizeof(hash_key_move_pos[0]); i++)
	{
		hash_key_move_pos[i] = random.random() % HASH_KEY_MAX;
	}

	// 重み初期化
	for (int i = 0; i < HASH_KEY_MAX; i++)
	{
		response_pattern_weight[i] = 1.0f;
	}
	for (int i = 0; i < HASH_KEY_MAX; i++)
	{
		nonresponse_pattern_weight[i] = 1.0f;
	}
	response_match_weight = 1.0f;

	// 衝突
	memset(response_pattern_collision, 0, sizeof(ResponsePatternVal) * HASH_KEY_MAX);
	memset(nonresponse_pattern_collision, 0, sizeof(NonResponsePatternVal) * HASH_KEY_MAX);
}

// レスポンスパターン値回転
PatternVal rotate_response_patternval_color(const PatternVal val)
{
	//         [1 ]
	//     [2 ][3 ][4 ]
	// [5 ][6 ][  ][7 ][8 ]
	//     [9 ][10][11]
	//         [12]
	// から
	//         [5 ]
	//     [9 ][6 ][2 ]
	// [12][10][  ][3 ][1 ]
	//     [11][7 ][4 ]
	//         [8 ]

	PatternVal rot = 0;

	// 1 → 5
	rot |= (val & 0b000000000001) << (5 - 1);
	// 2 → 9
	rot |= (val & 0b000000000010) << (9 - 2);
	// 3 → 6
	rot |= (val & 0b000000000100) << (6 - 3);
	// 4 → 2
	rot |= (val & 0b000000001000) >> (4 - 2);
	// 5 → 12
	rot |= (val & 0b000000010000) << (12 - 5);
	// 6 → 10
	rot |= (val & 0b000000100000) << (10 - 6);
	// 7 → 3
	rot |= (val & 0b000001000000) >> (7 - 3);
	// 8 → 1
	rot |= (val & 0b000010000000) >> (8 - 1);
	// 9 → 11
	rot |= (val & 0b000100000000) << (11 - 9);
	// 10 → 7
	rot |= (val & 0b001000000000) >> (10 - 7);
	// 11 → 4
	rot |= (val & 0b010000000000) >> (11 - 4);
	// 12 → 8
	rot |= (val & 0b100000000000) >> (12 - 8);

	return rot;
}

// レスポンスパターン値対称形(上下反転)
PatternVal mirror_response_patternval_color(const PatternVal val)
{
	//         [1 ]
	//     [2 ][3 ][4 ]
	// [5 ][6 ][  ][7 ][8 ]
	//     [9 ][10][11]
	//         [12]
	// から
	//         [12]
	//     [9 ][10][11]
	// [5 ][6 ][  ][7 ][8 ]
	//     [2 ][3 ][4 ]
	//         [1 ]

	PatternVal rot = 0;

	// 1 → 12
	rot |= (val & 0b000000000001) << (12 - 1);
	// 2 → 9
	rot |= (val & 0b000000000010) << (9 - 2);
	// 3 → 10
	rot |= (val & 0b000000000100) << (10 - 3);
	// 4 → 11
	rot |= (val & 0b000000001000) << (11 - 4);
	// 9 → 2
	rot |= (val & 0b000100000000) >> (9 - 2);
	// 10 → 3
	rot |= (val & 0b001000000000) >> (10 - 3);
	// 11 → 4
	rot |= (val & 0b010000000000) >> (11 - 4);
	// 12 → 1
	rot |= (val & 0b100000000000) >> (12 - 1);

	return rot;
}

// レスポンスパターン呼吸点値回転
PatternVal rotate_response_patternval_liberties(const PatternVal val)
{
	//         [1 ]
	//     [2 ][3 ][4 ]
	// [5 ][6 ][  ][7 ][8 ]
	//     [9 ][10][11]
	//         [12]
	// から
	//         [5 ]
	//     [9 ][6 ][2 ]
	// [12][10][  ][3 ][1 ]
	//     [11][7 ][4 ]
	//         [8 ]

	PatternVal rot = 0;

	// 1 → 5
	rot |= (val & 0b000000000000000000000011) << ((5 - 1) * 2);
	// 2 → 9
	rot |= (val & 0b000000000000000000001100) << ((9 - 2) * 2);
	// 3 → 6
	rot |= (val & 0b000000000000000000110000) << ((6 - 3) * 2);
	// 4 → 2
	rot |= (val & 0b000000000000000011000000) >> ((4 - 2) * 2);
	// 5 → 12
	rot |= (val & 0b000000000000001100000000) << ((12 - 5) * 2);
	// 6 → 10
	rot |= (val & 0b000000000000110000000000) << ((10 - 6) * 2);
	// 7 → 3
	rot |= (val & 0b000000000011000000000000) >> ((7 - 3) * 2);
	// 8 → 1
	rot |= (val & 0b000000001100000000000000) >> ((8 - 1) * 2);
	// 9 → 11
	rot |= (val & 0b000000110000000000000000) << ((11 - 9) * 2);
	// 10 → 7
	rot |= (val & 0b000011000000000000000000) >> ((10 - 7) * 2);
	// 11 → 4
	rot |= (val & 0b001100000000000000000000) >> ((11 - 4) * 2);
	// 12 → 8
	rot |= (val & 0b110000000000000000000000) >> ((12 - 8) * 2);

	return rot;
}

// レスポンスパターン呼吸点値対称形(上下反転)
PatternVal mirror_response_patternval_liberties(const PatternVal val)
{
	//         [1 ]
	//     [2 ][3 ][4 ]
	// [5 ][6 ][  ][7 ][8 ]
	//     [9 ][10][11]
	//         [12]
	// から
	//         [12]
	//     [9 ][10][11]
	// [5 ][6 ][  ][7 ][8 ]
	//     [2 ][3 ][4 ]
	//         [1 ]

	PatternVal rot = 0;

	// 1 → 12
	rot |= (val & 0b000000000000000000000011) << ((12 - 1) * 2);
	// 2 → 9
	rot |= (val & 0b000000000000000000001100) << ((9 - 2) * 2);
	// 3 → 10
	rot |= (val & 0b000000000000000000110000) << ((10 - 3) * 2);
	// 4 → 11
	rot |= (val & 0b000000000000000011000000) << ((11 - 4) * 2);
	// 9 → 2
	rot |= (val & 0b000000110000000000000000) >> ((9 - 2) * 2);
	// 10 → 3
	rot |= (val & 0b000011000000000000000000) >> ((10 - 3) * 2);
	// 11 → 4
	rot |= (val & 0b001100000000000000000000) >> ((11 - 4) * 2);
	// 12 → 1
	rot |= (val & 0b110000000000000000000000) >> ((12 - 1) * 2);

	return rot;
}

// レスポンスパターンmove_pos値回転
PatternVal rotate_response_patternval_move_pos(const PatternVal val)
{
	// [0 ][1 ][2 ][3 ][4 ]
	// [5 ][6 ][7 ][8 ][9 ]
	// [10][11][12][13][14]
	// [15][16][17][18][19]
	// [20][21][22][23][24]
	// から
	// [20][15][10][5 ][0 ]
	// [21][16][11][6 ][1 ]
	// [22][17][12][7 ][2 ]
	// [23][18][13][8 ][3 ]
	// [24][19][14][9 ][4 ]

	const PatternVal rot_tbl[] = { 20, 15, 10, 5, 0, 21, 16, 11, 6, 1, 22, 17, 12, 7, 2, 23, 18, 13, 8, 3, 24, 19, 14, 9, 4 };

	return rot_tbl[val];
}

// レスポンスパターンmove_pos値対称形(上下反転)
PatternVal mirror_response_patternval_move_pos(const PatternVal val)
{
	// [0 ][1 ][2 ][3 ][4 ]
	// [5 ][6 ][7 ][8 ][9 ]
	// [10][11][12][13][14]
	// [15][16][17][18][19]
	// [20][21][22][23][24]
	// から
	// [20][21][22][23][24]
	// [15][16][17][18][19]
	// [10][11][12][13][14]
	// [5 ][6 ][7 ][8 ][9 ]
	// [0 ][1 ][2 ][3 ][4 ]

	const PatternVal mirror_tbl[] = { 20, 21, 22, 23, 24, 15, 16, 17, 18, 19, 10, 11, 12, 13, 14, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4 };

	return mirror_tbl[val];
}

// レスポンスパターン値回転
ResponsePatternVal rotate_respons_pattern(const ResponsePatternVal& val)
{
	return{ rotate_response_patternval_color(val.black), rotate_response_patternval_color(val.white), rotate_response_patternval_liberties(val.liberties), rotate_response_patternval_move_pos(val.move_pos) };
}

// レスポンスパターン値対称形(上下反転)
ResponsePatternVal mirror_respons_pattern(const ResponsePatternVal& val)
{
	return{ mirror_response_patternval_color(val.black), mirror_response_patternval_color(val.white), mirror_response_patternval_liberties(val.liberties), mirror_response_patternval_move_pos(val.move_pos) };
}

// ノンレスポンスパターン値回転
PatternVal rotate_nonresponse_patternval_color(const PatternVal val)
{
	// [1][2][3]
	// [4][ ][5]
	// [6][7][8]
	// から
	// [6][4][1]
	// [7][ ][2]
	// [8][5][3]

	PatternVal rot = 0;

	// 1 → 6
	rot |= (val & 0b00000001) << (6 - 1);
	// 2 → 4
	rot |= (val & 0b00000010) << (4 - 2);
	// 3 → 1
	rot |= (val & 0b00000100) >> (3 - 1);
	// 4 → 7
	rot |= (val & 0b00001000) << (7 - 4);
	// 5 → 2
	rot |= (val & 0b00010000) >> (5 - 2);
	// 6 → 8
	rot |= (val & 0b00100000) << (8 - 6);
	// 7 → 5
	rot |= (val & 0b01000000) >> (7 - 5);
	// 8 → 3
	rot |= (val & 0b10000000) >> (8 - 3);

	return rot;
}

// ノンレスポンスパターン値対称形(上下反転)
PatternVal mirror_nonresponse_patternval_color(const PatternVal val)
{
	// [1][2][3]
	// [4][ ][5]
	// [6][7][8]
	// から
	// [6][7][8]
	// [4][ ][5]
	// [1][2][3]

	PatternVal rot = 0;

	// 1 → 6
	rot |= (val & 0b00000001) << (6 - 1);
	// 2 → 7
	rot |= (val & 0b00000010) << (7 - 2);
	// 3 → 8
	rot |= (val & 0b00000100) << (8 - 3);
	// 6 → 1
	rot |= (val & 0b00100000) >> (6 - 1);
	// 7 → 2
	rot |= (val & 0b01000000) >> (7 - 2);
	// 8 → 3
	rot |= (val & 0b10000000) >> (8 - 3);

	return rot;
}

// ノンレスポンスパターン値回転
PatternVal rotate_nonresponse_patternval_liberties(const PatternVal val)
{
	// [1][2][3]
	// [4][ ][5]
	// [6][7][8]
	// から
	// [6][4][1]
	// [7][ ][2]
	// [8][5][3]

	PatternVal rot = 0;

	// 1 → 6
	rot |= (val & 0b0000000000000011) << ((6 - 1) * 2);
	// 2 → 4
	rot |= (val & 0b0000000000001100) << ((4 - 2) * 2);
	// 3 → 1
	rot |= (val & 0b0000000000110000) >> ((3 - 1) * 2);
	// 4 → 7
	rot |= (val & 0b0000000011000000) << ((7 - 4) * 2);
	// 5 → 2
	rot |= (val & 0b0000001100000000) >> ((5 - 2) * 2);
	// 6 → 8
	rot |= (val & 0b0000110000000000) << ((8 - 6) * 2);
	// 7 → 5
	rot |= (val & 0b0011000000000000) >> ((7 - 5) * 2);
	// 8 → 3
	rot |= (val & 0b1100000000000000) >> ((8 - 3) * 2);

	return rot;
}

// ノンレスポンスパターン値対称形(上下反転)
PatternVal mirror_nonresponse_patternval_liberties(const PatternVal val)
{
	// [1][2][3]
	// [4][ ][5]
	// [6][7][8]
	// から
	// [6][7][8]
	// [4][ ][5]
	// [1][2][3]

	PatternVal rot = 0;

	// 1 → 6
	rot |= (val & 0b0000000000000011) << ((6 - 1) * 2);
	// 2 → 7
	rot |= (val & 0b0000000000001100) << ((7 - 2) * 2);
	// 3 → 8
	rot |= (val & 0b0000000000110000) << ((8 - 3) * 2);
	// 6 → 1
	rot |= (val & 0b0000110000000000) >> ((6 - 1) * 2);
	// 7 → 2
	rot |= (val & 0b0011000000000000) >> ((7 - 2) * 2);
	// 8 → 3
	rot |= (val & 0b1100000000000000) >> ((8 - 3) * 2);

	return rot;
}

// ノンレスポンスパターン値回転
NonResponsePatternVal rotate_nonrespons_pattern(const NonResponsePatternVal& val)
{
	return{ rotate_nonresponse_patternval_color(val.black), rotate_nonresponse_patternval_color(val.white), rotate_nonresponse_patternval_liberties(val.liberties) };
}

// ノンレスポンスパターン値対称形(上下反転)
NonResponsePatternVal mirror_nonrespons_pattern(const NonResponsePatternVal& val)
{
	return{ mirror_nonresponse_patternval_color(val.black), mirror_nonresponse_patternval_color(val.white), mirror_nonresponse_patternval_liberties(val.liberties) };
}


// レスポンスパターン用ハッシュキー値取得
inline HashKey get_hash_key_response_pattern(const ResponsePatternVal& val)
{
	return hash_key_pattern_black[val.black] ^ hash_key_pattern_white[val.white] ^ hash_key_pattern_liberties[val.liberties] ^ hash_key_move_pos[val.move_pos];
}

// レスポンスパターン用ハッシュキー値取得(回転、対称形の最小値)
HashKey get_hash_key_response_pattern_min(const ResponsePatternVal& val, ResponsePatternVal* min)
{
	HashKey hashkey_min = get_hash_key_response_pattern(val);
	*min = val;

	// 90度回転
	ResponsePatternVal rot = rotate_respons_pattern(val);
	HashKey hashkey = get_hash_key_response_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 180度回転
	rot = rotate_respons_pattern(rot);
	hashkey = get_hash_key_response_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 270度回転
	rot = rotate_respons_pattern(rot);
	hashkey = get_hash_key_response_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 上下反転
	rot = mirror_respons_pattern(val);
	hashkey = get_hash_key_response_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 90度回転
	rot = rotate_respons_pattern(rot);
	hashkey = get_hash_key_response_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 180度回転
	rot = rotate_respons_pattern(rot);
	hashkey = get_hash_key_response_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 270度回転
	rot = rotate_respons_pattern(rot);
	hashkey = get_hash_key_response_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	return hashkey_min;
}

// ノンレスポンスパターン用ハッシュキー値取得
HashKey get_hash_key_nonresponse_pattern(const NonResponsePatternVal& val)
{
	return hash_key_pattern_black[val.black] ^ hash_key_pattern_white[val.white] ^ hash_key_pattern_liberties[val.liberties];
}

// ノンレスポンスパターン用ハッシュキー値取得(回転、対称形の最小値)
HashKey get_hash_key_nonresponse_pattern_min(const NonResponsePatternVal& val, NonResponsePatternVal* min)
{
	HashKey hashkey_min = get_hash_key_nonresponse_pattern(val);
	*min = val;

	// 90度回転
	NonResponsePatternVal rot = rotate_nonrespons_pattern(val);
	HashKey hashkey = get_hash_key_nonresponse_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 180度回転
	rot = rotate_nonrespons_pattern(rot);
	hashkey = get_hash_key_nonresponse_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 270度回転
	rot = rotate_nonrespons_pattern(rot);
	hashkey = get_hash_key_nonresponse_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 上下反転
	rot = mirror_nonrespons_pattern(val);
	hashkey = get_hash_key_nonresponse_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 90度回転
	rot = rotate_nonrespons_pattern(rot);
	hashkey = get_hash_key_nonresponse_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 180度回転
	rot = rotate_nonrespons_pattern(rot);
	hashkey = get_hash_key_nonresponse_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	// 270度回転
	rot = rotate_nonrespons_pattern(rot);
	hashkey = get_hash_key_nonresponse_pattern(rot);
	if (hashkey < hashkey_min)
	{
		hashkey_min = hashkey;
		*min = rot;
	}

	return hashkey_min;
}

bool is_sido(char* next)
{
	char* ev = strstr(next, "EV[");
	if (ev == NULL)
	{
		return false;
	}
	if (ev[9] == -26 && ev[10] == -116 && ev[11] == -121)
	{
		return true;
	}
	return false;
}

Color get_win_from_re(char* next, const wchar_t* infile)
{
	char* re = strstr(next, "RE[");
	if (re == NULL)
	{
		fprintf(stderr, "RE not found. %S\n", infile);
		return 0;
	}

	char win = re[3];
	if (win == 'b' || win == 'L' || win == -23 || re[15] == -23 || re[12] == -23 || re[6] == -23 || re[19] == -23 || re[13] == -23 || re[11] == -23 || re[9] == -23 || re[4] == -23 || re[9] == 'B' || re[9] == -19 || re[10] == -19 || re[8] == -19 || re[21] == -19 || win == -19 || re[15] == -19 || re[16] == -19)
	{
		win = 'B';
	}
	else if (win == 'w' || win == 'R' || win == -25 || re[15] == -25 || re[12] == -25 || re[6] == -25 || re[19] == -25 || re[13] == -25 || re[11] == -25 || re[9] == -25 || re[4] == -25 || re[9] == 'W' || re[9] == -21 || re[10] == -21 || re[8] == -21 || re[21] == -21 || win == -21 || re[15] == -21 || re[16] == -21)
	{
		win = 'W';
	}
	if (win != 'B' && win != 'W')
	{
		if (win != 'J' && win != 'j' && win != 'V' && win != -27 && win != -26 && win != 'd' && win != '0' && win != '?' && win != -29 && win != -28 && win != ']' && win != 'u' && win != 'U' && win != 's')
		{
			fprintf(stderr, "win illigal. %S\n", infile);
		}
		return 0;
	}

	return (win == 'B') ? BLACK : WHITE;
}

Color get_color_from_sgf(char* next)
{
	char c = next[0];
	Color color;
	if (c == 'B')
	{
		color = BLACK;
	}
	else if (c == 'W') {
		color = WHITE;
	}
	else {
		return 0;
	}
	return color;
}

XY get_xy_from_sgf(char* next)
{
	// PASS
	if (next[2] == ']' || next[2] == '?' || next[1] == ']')
	{
		return PASS;
	}

	int x = next[2] - 'a' + 1;
	int y = next[3] - 'a' + 1;
	XY xy = get_xy(x, y);
	//printf("%s, x, y = %d, %d\n", next, x, y);

	if (next[1] == '\\')
	{
		x = next[3] - 'a' + 1;
		y = next[4] - 'a' + 1;
		xy = get_xy(x, y);
	}
	else if (next[2] == -28)
	{
		xy = get_xy(1, 1);
	}

	return xy;
}

inline int get_liberty_val(int liberty_num)
{
	return (liberty_num >= 3) ? 3 : liberty_num;
}

HashKey response_pattern(const Board& board, const XY xy, Color color, ResponsePatternVal* val)
{
	// 直前の手の12ポイント範囲内か
	XY d = xy - board.pre_xy;
	XY dx = get_x(d);
	XY dy = get_y(d);
	if (abs(dx) + abs(dy) > 2)
	{
		return 0;
	}

	PatternVal color_pattern[] = { 0, 0, 0, 0 }; // 色別パターン値
	PatternVal liberties = 0; // 呼吸点のキー値

	// 1段目
	XY xyp = board.pre_xy - BOARD_WIDTH * 2;
	if (xyp > BOARD_WIDTH && !board.is_empty(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 1);
		liberties = get_liberty_val(group.liberty_num);
	}

	// 2段目
	xyp = board.pre_xy - BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 2);
		liberties |= get_liberty_val(group.liberty_num) << 2;
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 3);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 2);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 4);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 3);
	}

	// 3段目
	xyp = board.pre_xy - 2;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp) && !board.is_offboard(xyp + 1))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 5);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 4);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 6);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 5);
	}
	xyp += 2;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 7);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 6);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp) && !board.is_offboard(xyp - 1))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 8);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 7);
	}

	// 4段目
	xyp = board.pre_xy + BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 9);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 8);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 10);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 9);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 11);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 10);
	}

	// 5段目
	xyp = board.pre_xy + BOARD_WIDTH * 2;
	if (xyp < BOARD_MAX - BOARD_WIDTH && !board.is_empty(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 12);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 11);
	}

	// 黒を基準にする
	Color color_black = (color == BLACK) ? BLACK : WHITE;
	Color color_white = opponent(color_black);

	PatternVal move_pos = (dy + 2) * 5 + (dx + 2);
	return get_hash_key_response_pattern_min({ color_pattern[color_black], color_pattern[color_white], liberties, move_pos }, val);
}

HashKey nonresponse_pattern(const Board& board, const XY xy, Color color, NonResponsePatternVal* val)
{
	PatternVal color_pattern[] = { 0, 0, 0, 0 }; // 色別パターン値
	PatternVal liberties = 0; // 呼吸点のキー値

	 // 1段目
	XY xyp = xy - BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 1);
		liberties |= get_liberty_val(group.liberty_num);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 2);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 2);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 3);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 3);
	}

	// 2段目
	xyp = xy - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 4);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 4);
	}
	xyp += 2;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 5);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 5);
	}

	// 3段目
	xyp = xy + BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 6);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 6);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 7);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 7);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		_bittestandset((long*)&color_pattern[group.color], 8);
		liberties |= get_liberty_val(group.liberty_num) << (2 * 8);
	}

	// 黒を基準にする
	Color color_black = (color == BLACK) ? BLACK : WHITE;
	Color color_white = opponent(color_black);

	return get_hash_key_nonresponse_pattern_min({ color_pattern[color_black], color_pattern[color_white], liberties }, val);
}

void learn_pattern_sgf(const wchar_t* infile, int &learned_position_num)
{
	FILE* fp = _wfopen(infile, L"r");
	char buf[10000];
	// 1行目読み飛ばし
	fgets(buf, sizeof(buf), fp);
	// 2行目
	fgets(buf, sizeof(buf), fp);

	// ;で区切る
	char* next = strtok(buf, ";");

	// 指導碁除外
	if (is_sido(next))
	{
		return;
	}

	// 結果取得
	Color win = get_win_from_re(next, infile);
	if (win == 0)
	{
		fclose(fp);
		return;
	}

	Board board(19);

	int turn = 0;
	float loss = 0;
	int loss_cnt = 0;
	while ((next = strtok(NULL, ";")) != NULL)
	{
		Color color = get_color_from_sgf(next);
		if (color == 0) {
			continue;
		}

		XY xy = get_xy_from_sgf(next);
		if (xy == PASS)
		{
			continue;
		}

		// 勝ったプレイヤー
		if (color == win && turn >= 10)
		{
			float e_sum = 0;
			float e_y = 0;
			float e_etc[19 * 19] = { 0 };

			struct Key
			{
				HashKey response_key;
				HashKey nonresponse_key;
			};

			Key key_y; // 教師データのキー
			Key keys[19 * 19]; // 教師データの以外のキー
			int num = 0;

			// 候補手一覧
			for (XY txy = BOARD_WIDTH + 1; txy < BOARD_MAX - BOARD_WIDTH; txy++)
			{
				if (board.is_empty(txy) && board.is_legal(txy, color, false) == SUCCESS)
				{
					// 候補手パターン
					// レスポンスパターン
					ResponsePatternVal response_val = response_pattern_collision0;
					HashKey response_key = response_pattern(board, txy, color, &response_val);

					// 衝突チェック
					if (response_pattern_collision[response_key] == response_pattern_collision0)
					{
						response_pattern_collision[response_key] = response_val;
					}
					else if (response_pattern_collision[response_key] != response_val)
					{
						// 衝突
						assert(false);
					}

					// ノンレスポンスパターン
					NonResponsePatternVal nonresponse_val = nonresponse_pattern_collision0;
					HashKey nonresponse_key = nonresponse_pattern(board, txy, color, &nonresponse_val);

					// 衝突チェック
					if (nonresponse_pattern_collision[nonresponse_key] == nonresponse_pattern_collision0)
					{
						nonresponse_pattern_collision[nonresponse_key] = nonresponse_val;
					}
					else if (nonresponse_pattern_collision[nonresponse_key] != nonresponse_val)
					{
						// 衝突
						assert(false);
					}

					// パラメータ更新準備
					// 重みの線形和
					float weight_sum = nonresponse_pattern_weight[nonresponse_key];
					if (response_key != 0)
					{
						weight_sum += response_match_weight;
						weight_sum += response_pattern_weight[response_key];
					}

					// 各手のsoftmaxを計算
					float e = expf(weight_sum);
					e_sum += e;

					// 教師データと一致する場合
					if (txy == xy)
					{
						e_y = e;
						key_y.response_key = response_key;
						key_y.nonresponse_key = nonresponse_key;
					}
					else {
						e_etc[num] = e;
						keys[num].response_key = response_key;
						keys[num].nonresponse_key = nonresponse_key;
						num++;
					}
				}
			}

			// 教師データと一致する手のsoftmax
			float y = e_y / e_sum;

			// 教師データと一致する手のパラメータ更新
			if (key_y.nonresponse_key == 0)
			{
				// 空白パターンは固定値として更新しない
			}
			else
			{
				nonresponse_pattern_weight[key_y.nonresponse_key] -= eta * (y - 1.0f) * nonresponse_pattern_weight[key_y.nonresponse_key];
			}
			if (key_y.response_key != 0)
			{
				response_match_weight -= eta * (y - 1.0f) * response_match_weight;
				response_pattern_weight[key_y.response_key] -= eta * (y - 1.0f) * response_pattern_weight[key_y.response_key];
			}

			// 損失関数
			loss += -logf(y);
			loss_cnt++;

			// 教師データと一致しない手のパラメータ更新
			for (int i = 0; i < num; i++)
			{
				float y_etc = e_etc[i] / e_sum;
				if (keys[i].nonresponse_key == 0)
				{
					// 空白パターンは固定値として更新しない
				}
				else
				{
					nonresponse_pattern_weight[keys[i].nonresponse_key] -= eta * y_etc * nonresponse_pattern_weight[keys[i].nonresponse_key];
				}
				if (keys[i].response_key != 0)
				{
					response_match_weight -= eta * y_etc * response_match_weight;
					response_pattern_weight[keys[i].response_key] -= eta * y_etc * response_pattern_weight[keys[i].response_key];
				}
			}

			learned_position_num++;
		}

		board.move(xy, color, true);
		turn++;
	}

	// 損失関数の平均値表示
	printf("loss = %f\n", loss / loss_cnt);

	fclose(fp);
}

void learn_pattern(const wchar_t** dirs, const int dir_num)
{
	int learned_game_num = 0; // 学習局数
	int learned_position_num = 0; // 学習局面数

	// 棋譜を読み込んで学習
	for (int i = 0; i < dir_num; i++)
	{
		// 入力ファイル一覧
		wstring finddir(dirs[i]);
		WIN32_FIND_DATA win32fd;
		HANDLE hFind = FindFirstFile((finddir + L"\\*.sgf").c_str(), &win32fd);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "dir open error. %S\n", dirs[i]);
			return;
		}

		do {
			if (win32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				continue;
			}

			// パターン学習
			learn_pattern_sgf((finddir + L"\\" + win32fd.cFileName).c_str(), learned_position_num);
			learned_game_num++;
		} while (FindNextFile(hFind, &win32fd));
	}
}

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2)
	{
		return 1;
	}

	init_hash_table_and_weight();
	learn_pattern((const wchar_t**)argv + 1, argc - 1);

	return 0;
}