#include <windows.h>
#include <string>
#include <cassert>
#include <map>
#include <vector>

#include "../Board.h"
#include "../Random.h"

using namespace std;

typedef unsigned char PatternVal8;
typedef unsigned int PatternVal32;
typedef unsigned long long PatternVal64;
typedef unsigned int HashKey;

// 学習係数
float eta = 0.01;

// パターン用ハッシュテーブル 12points
const int HASH_KEY_MAX_PATTERN = 12 / 2; // color,libertiesのセット4byte * 2単位
HashKey hash_key_pattern[HASH_KEY_MAX_PATTERN][256];
HashKey hash_key_move_pos[5 * 5];

const int HASH_KEY_BIT = 24;
const int HASH_KEY_MAX = 1 << HASH_KEY_BIT;
const int HASH_KEY_MASK = HASH_KEY_MAX - 1;

struct ResponsePatternVal
{
	__declspec(align(8)) struct Vals {
		PatternVal8 color_liberties[12 / 2];
		PatternVal8 move_pos;
	};

	union {
		PatternVal64 val64;
		Vals vals;
	};

	ResponsePatternVal() {}

	ResponsePatternVal(const ResponsePatternVal& val) {
		val64 = val.val64;
	}

	ResponsePatternVal(const unsigned long long val64) {
		this->val64 = val64;
	}

	ResponsePatternVal(const PatternVal64 color_liberties, const PatternVal8 move_pos) {
		this->val64 = color_liberties;
		this->vals.move_pos = move_pos;
	}

	bool operator ==(const ResponsePatternVal& val) const {
		return val64 == val.val64;
	}

	bool operator !=(const ResponsePatternVal& val) const {
		return val64 != val.val64;
	}

	bool operator <(const ResponsePatternVal& val) const {
		return val64 < val.val64;
	}

	// 90度回転
	ResponsePatternVal rotate() const {
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

		ResponsePatternVal rot = 0;

		// 石の色、呼吸点
		// 1 → 8
		// 4 → 11
		rot.val64 |= (val64 & 0b000000000000000000000000000000001111000000001111) << ((8 - 1)/*(11 - 4)*/ * 4);
		// 2 → 4
		rot.val64 |= (val64 & 0b000000000000000000000000000000000000000011110000) << ((4 - 2) * 4);
		// 3 → 7
		// 8 → 12
		rot.val64 |= (val64 & 0b000000000000000011110000000000000000111100000000) << ((7 - 3)/*(12 - 8)*/ * 4);
		// 5 → 1
		// 10 → 6
		rot.val64 |= (val64 & 0b000000001111000000000000000011110000000000000000) >> ((5 - 1)/*(10 - 6)*/ * 4);
		// 6 → 3
		rot.val64 |= (val64 & 0b000000000000000000000000111100000000000000000000) >> ((6 - 3) * 4);
		// 7 → 10
		rot.val64 |= (val64 & 0b000000000000000000001111000000000000000000000000) << ((10 - 7) * 4);
		// 9 → 2
		// 12 → 5
		rot.val64 |= (val64 & 0b111100000000111100000000000000000000000000000000) >> ((9 - 2)/*(12 - 5)*/ * 4);
		// 11 → 9
		rot.val64 |= (val64 & 0b000011110000000000000000000000000000000000000000) >> ((11 - 9) * 4);

		// move_pos
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

		const PatternVal8 rot_tbl[] = { 4, 9, 14, 19, 24, 3, 8, 13, 18, 23, 2, 7, 12, 17, 22, 1, 6, 11, 16, 21, 0, 5, 10, 15, 20 };
		rot.vals.move_pos |= rot_tbl[vals.move_pos];

		return rot;
	}

	// 上下反転
	ResponsePatternVal vmirror() const {
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

		ResponsePatternVal rot = 0;

		// 石の色
		// 1 → 12
		rot.val64 |= (val64 & 0b000000000000000000000000000000000000000000001111) << ((12 - 1) * 4);
		// 2 → 9
		// 3 → 10
		// 4 → 11
		rot.val64 |= (val64 & 0b000000000000000000000000000000001111111111110000) << ((9 - 2)/*(10 - 3)*//*(11 - 4)*/ * 4);
		// 9 → 2
		// 10 → 3
		// 11 → 4
		rot.val64 |= (val64 & 0b000011111111111100000000000000000000000000000000) >> ((9 - 2)/*(10 - 3)*//*(11 - 4)*/ * 4);
		// 12 → 1
		rot.val64 |= (val64 & 0b111100000000000000000000000000000000000000000000) >> ((12 - 1) * 4);

		// move_pos
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

		const PatternVal8 mirror_tbl[] = { 20, 21, 22, 23, 24, 15, 16, 17, 18, 19, 10, 11, 12, 13, 14, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4 };
		rot.vals.move_pos = mirror_tbl[vals.move_pos];

		return rot;
	}

	// 左右反転
	ResponsePatternVal hmirror() const {
		//         [1 ]
		//     [2 ][3 ][4 ]
		// [5 ][6 ][  ][7 ][8 ]
		//     [9 ][10][11]
		//         [12]
		// から
		//         [1 ]
		//     [4 ][3 ][2 ]
		// [8 ][7 ][  ][6 ][5 ]
		//     [11][10][9 ]
		//         [12]

		ResponsePatternVal rot = 0;

		// 石の色
		// 2 → 4
		// 9 → 11
		rot.val64 |= (val64 & 0b000000000000111100000000000000000000000011110000) >> ((4 - 2)/*(11 - 9)*/ * 4);
		// 5 → 8
		rot.val64 |= (val64 & 0b000000000000000000000000000011110000000000000000) >> ((8 - 5) * 4);
		// 6 → 7
		rot.val64 |= (val64 & 0b000000000000000000000000111100000000000000000000) >> ((7 - 6) * 4);
		// 4 → 2
		// 11 → 9
		rot.val64 |= (val64 & 0b000011110000000000000000000000001111000000000000) >> ((4 - 2)/*(11 - 9)*/ * 4);
		// 7 → 6
		rot.val64 |= (val64 & 0b000000000000000000001111000000000000000000000000) >> ((7 - 6) * 4);
		// 8 → 5
		rot.val64 |= (val64 & 0b000000000000000011110000000000000000000000000000) >> ((8 - 5) * 4);

		// move_pos
		// [0 ][1 ][2 ][3 ][4 ]
		// [5 ][6 ][7 ][8 ][9 ]
		// [10][11][12][13][14]
		// [15][16][17][18][19]
		// [20][21][22][23][24]
		// から
		// [4 ][3 ][2 ][1 ][0 ]
		// [9 ][8 ][7 ][6 ][5 ]
		// [14][13][12][11][10]
		// [19][18][17][16][15]
		// [24][23][22][21][20]

		const PatternVal8 mirror_tbl[] = { 4, 3, 2, 1, 0, 9, 8, 7, 6, 5, 14, 13, 12, 11, 10, 19, 18, 17, 16, 15, 24, 23, 22, 21, 20 };
		rot.vals.move_pos = mirror_tbl[vals.move_pos];

		return rot;
	}
};

struct NonResponsePatternVal
{
	__declspec(align(8)) struct Vals {
		PatternVal8 color_liberties[8 / 2];
	};

	union {
		PatternVal32 val32;
		Vals vals;
	};

	NonResponsePatternVal() {}

	NonResponsePatternVal(const NonResponsePatternVal& val) {
		val32 = val.val32;
	}

	NonResponsePatternVal(const PatternVal32 val32) {
		this->val32 = val32;
	}

	bool operator ==(const NonResponsePatternVal& val) const {
		return val32 == val.val32;
	}

	bool operator !=(const NonResponsePatternVal& val) const {
		return val32 != val.val32;
	}

	bool operator <(const NonResponsePatternVal& val) const {
		return val32 < val.val32;
	}

	// 90度回転
	NonResponsePatternVal rotate() const {
		// [1][2][3]
		// [4][ ][5]
		// [6][7][8]
		// から
		// [6][4][1]
		// [7][ ][2]
		// [8][5][3]

		NonResponsePatternVal rot = 0;

		// 石の色
		// 1 → 3
		// 5 → 7
		rot.val32 |= (val32 & 0b00000000000011110000000000001111) << ((3 - 1)/*(7 - 5)*/ * 4);
		// 2 → 5
		rot.val32 |= (val32 & 0b00000000000000000000000011110000) << ((5 - 2) * 4);
		// 3 → 8
		rot.val32 |= (val32 & 0b00000000000000000000111100000000) << ((8 - 3) * 4);
		// 4 → 2
		// 8 → 6
		rot.val32 |= (val32 & 0b11110000000000001111000000000000) >> ((4 - 2)/*(8 - 6)*/ * 4);
		// 6 → 1
		rot.val32 |= (val32 & 0b00000000111100000000000000000000) >> ((6 - 1) * 4);
		// 7 → 4
		rot.val32 |= (val32 & 0b00001111000000000000000000000000) >> ((7 - 4) * 4);

		return rot;
	}

	// 上下反転
	NonResponsePatternVal vmirror() const {
		// [1][2][3]
		// [4][ ][5]
		// [6][7][8]
		// から
		// [6][7][8]
		// [4][ ][5]
		// [1][2][3]

		NonResponsePatternVal rot = 0;

		// 1 → 6
		// 2 → 7
		// 3 → 8
		rot.val32 |= (val32 & 0b00000000000000000000111111111111) << ((6 - 1)/*(7 - 2)*//*(8 - 3)*/ * 4);
		// 6 → 1
		// 7 → 2
		// 8 → 3
		rot.val32 |= (val32 & 0b11111111111100000000000000000000) >> ((6 - 1)/*(7 - 2)*//*(8 - 3)*/ * 4);

		return rot;
	}

	// 左右反転
	NonResponsePatternVal hmirror() const {
		// [1][2][3]
		// [4][ ][5]
		// [6][7][8]
		// から
		// [3][2][1]
		// [5][ ][4]
		// [8][7][6]

		NonResponsePatternVal rot = 0;

		// 1 → 3
		// 6 → 8
		rot.val32 |= (val32 & 0b00000000111100000000000000001111) << ((3 - 1)/*(8 - 6)*/ * 4);
		// 4 → 5
		rot.val32 |= (val32 & 0b00000000000011110000000000000000) << ((5 - 4) * 4);
		// 3 → 1
		// 8 → 6
		rot.val32 |= (val32 & 0b11110000000000000000111100000000) >> ((3 - 1)/*(8 - 6)*/ * 4);
		// 5 → 4
		rot.val32 |= (val32 & 0b00000000000011110000000000000000) >> ((5 - 4) * 4);

		return rot;
	}
};

// アタリを防ぐ手の重み
float save_atari_weight;

// 直前の手と隣接
float neighbour_weight;

// レスポンスマッチの重み
float response_match_weight;

// レスポンスパターンの重み
map<ResponsePatternVal, float> response_pattern_weight;

// ノンレスポンスパターンの重み
map<NonResponsePatternVal, float> nonresponse_pattern_weight;

// ハッシュキー衝突検出用
ResponsePatternVal response_pattern_collision[HASH_KEY_MAX];
NonResponsePatternVal nonresponse_pattern_collision[HASH_KEY_MAX];
ResponsePatternVal response_pattern_collision0 = { 0 };
NonResponsePatternVal nonresponse_pattern_collision0 = { 0 };

// 各色のパターン用ハッシュキー値生成
void init_hash_table_and_weight(const uint64_t seed) {
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

	// 衝突
	memset(response_pattern_collision, 0, sizeof(ResponsePatternVal) * HASH_KEY_MAX);
	memset(nonresponse_pattern_collision, 0, sizeof(NonResponsePatternVal) * HASH_KEY_MAX);
}


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

// レスポンスパターン用ハッシュキー値取得(回転、対称形の最小値)
HashKey get_hash_key_response_pattern_min(const ResponsePatternVal& val, ResponsePatternVal& min)
{
	min = val;

	// 90度回転
	ResponsePatternVal rot = val.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// 180度回転
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// 270度回転
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// 上下反転
	rot = val.vmirror();
	if (rot < min)
	{
		min = rot;
	}

	// 90度回転
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// 左右反転
	rot = val.hmirror();
	if (rot < min)
	{
		min = rot;
	}

	// 90度回転
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	return get_hash_key_response_pattern(min);
}

// ノンレスポンスパターン用ハッシュキー値取得
HashKey get_hash_key_nonresponse_pattern(const NonResponsePatternVal& val)
{
	return hash_key_pattern[0][val.vals.color_liberties[0]]
		^ hash_key_pattern[1][val.vals.color_liberties[1]]
		^ hash_key_pattern[2][val.vals.color_liberties[2]]
		^ hash_key_pattern[3][val.vals.color_liberties[3]];
}

// ノンレスポンスパターン用ハッシュキー値取得(回転、対称形の最小値)
HashKey get_hash_key_nonresponse_pattern_min(const NonResponsePatternVal& val, NonResponsePatternVal& min)
{
	min = val;

	// 90度回転
	NonResponsePatternVal rot = val.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// 180度回転
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// 270度回転
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// 上下反転
	rot = val.vmirror();
	if (rot < min)
	{
		min = rot;
	}

	// 90度回転
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// 左右反転
	rot = val.hmirror();
	if (rot < min)
	{
		min = rot;
	}

	// 90度回転
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	return get_hash_key_nonresponse_pattern(min);
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
	if (next[2] == ']' || next[2] == '?' || next[1] == ']' || next[2] == 't')
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

inline int get_liberty_val(const int liberty_num)
{
	return (liberty_num >= 3) ? 3 : liberty_num;
}

HashKey response_pattern(const Board& board, const XY xy, Color color, ResponsePatternVal& min)
{
	// 直前の手の12ポイント範囲内か
	XY d = xy - board.pre_xy;
	XY dx = get_x(d);
	XY dy = get_y(d);
	if (abs(dx) + abs(dy) > 2)
	{
		return 0;
	}

	// 黒を基準にする
	PatternVal64 color_mask = (color == BLACK) ? 0 : 0b11;

	ResponsePatternVal val = { 0 };

	// 1段目
	XY xyp = board.pre_xy - BOARD_WIDTH * 2;
	if (xyp > BOARD_WIDTH && !board.is_empty(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2));
	}

	// 2段目
	xyp = board.pre_xy - BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 1);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 2);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 3);
	}

	// 3段目
	xyp = board.pre_xy - 2;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp) && !board.is_offboard(xyp + 1))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 4);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 5);
	}
	xyp += 2;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 6);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp) && !board.is_offboard(xyp - 1))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 7);
	}

	// 4段目
	xyp = board.pre_xy + BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 8);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 9);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 10);
	}

	// 5段目
	xyp = board.pre_xy + BOARD_WIDTH * 2;
	if (xyp < BOARD_MAX - BOARD_WIDTH && !board.is_empty(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val64 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 11);
	}

	val.vals.move_pos = (dy + 2) * 5 + (dx + 2);
	return get_hash_key_response_pattern_min(val, min);
}

HashKey nonresponse_pattern(const Board& board, const XY xy, Color color, NonResponsePatternVal& min)
{
	// 黒を基準にする
	PatternVal32 color_mask = (color == BLACK) ? 0 : 0b11;

	NonResponsePatternVal val = { 0 };

	 // 1段目
	XY xyp = xy - BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2));
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 1);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 2);
	}

	// 2段目
	xyp = xy - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 3);
	}
	xyp += 2;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 4);
	}

	// 3段目
	xyp = xy + BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 5);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 6);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= ((group.color ^ color_mask) | (get_liberty_val(group.liberty_num) << 2)) << (4 * 7);
	}

	return get_hash_key_nonresponse_pattern_min(val, min);
}

bool is_neighbour(const Board& board, XY xy)
{
	XY dx = get_x(xy) - get_x(board.pre_xy);
	XY dy = get_y(xy) - get_y(board.pre_xy);

	return abs(dx) <= 1 && abs(dy) <= 1;
}

int learn_pattern_sgf(const wchar_t* infile, int &learned_position_num)
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
		return 0;
	}

	// 結果取得
	Color win = get_win_from_re(next, infile);
	if (win == 0)
	{
		fclose(fp);
		return 0;
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
				XY xy;
				ResponsePatternVal response_val;
				NonResponsePatternVal nonresponse_val;
			};

			Key key_y; // 教師データのキー
			Key keys[19 * 19]; // 教師データの以外のキー
			int num = 0;

			// アタリを助ける手を取得
			BitBoard<BOARD_BYTE_MAX> atari_save;
			board.get_atari_save(color, atari_save);

			// 候補手一覧
			for (XY txy = BOARD_WIDTH + 1; txy < BOARD_MAX - BOARD_WIDTH; txy++)
			{
				if (board.is_empty(txy) && board.is_legal(txy, color, false) == SUCCESS)
				{
					// 候補手パターン
					// レスポンスパターン
					ResponsePatternVal response_val = { 0 };
					response_pattern(board, txy, color, response_val);

					// ノンレスポンスパターン
					NonResponsePatternVal nonresponse_val = { 0 };
					nonresponse_pattern(board, txy, color, nonresponse_val);

					// パラメータ更新準備
					// 重みの線形和
					float weight_sum = nonresponse_pattern_weight[nonresponse_val];
					if (response_val != 0)
					{
						weight_sum += response_match_weight;
						weight_sum += response_pattern_weight[response_val];
					}
					// アタリを助ける手か
					if (atari_save.bit_test(txy))
					{
						weight_sum += save_atari_weight;
					}
					// 直前の手に隣接する手か
					if (is_neighbour(board, txy))
					{
						weight_sum += neighbour_weight;
					}

					// 各手のsoftmaxを計算
					float e = expf(weight_sum);
					e_sum += e;

					// 教師データと一致する場合
					if (txy == xy)
					{
						e_y = e;
						key_y.response_val = response_val;
						key_y.nonresponse_val = nonresponse_val;
					}
					else {
						e_etc[num] = e;
						keys[num].xy = txy;
						keys[num].response_val = response_val;
						keys[num].nonresponse_val = nonresponse_val;
						num++;
					}
				}
			}

			// 教師データと一致する手のsoftmax
			float y = e_y / e_sum;

			// 教師データと一致する手のパラメータ更新
			if (key_y.nonresponse_val == 0)
			{
				// 空白パターンは固定値として更新しない
			}
			else
			{
				nonresponse_pattern_weight[key_y.nonresponse_val] -= eta * (y - 1.0f) * nonresponse_pattern_weight[key_y.nonresponse_val];
			}
			if (key_y.response_val != 0)
			{
				response_match_weight -= eta * (y - 1.0f) * response_match_weight;
				response_pattern_weight[key_y.response_val] -= eta * (y - 1.0f) * response_pattern_weight[key_y.response_val];
			}
			// アタリを助ける手か
			if (atari_save.bit_test(xy))
			{
				save_atari_weight -= eta * (y - 1.0f) * save_atari_weight;
			}
			// 直前の手に隣接する手か
			if (is_neighbour(board, xy))
			{
				neighbour_weight -= eta * (y - 1.0f) * neighbour_weight;
			}

			// 損失関数
			loss += -logf(y);
			loss_cnt++;

			// 教師データと一致しない手のパラメータ更新
			for (int i = 0; i < num; i++)
			{
				float y_etc = e_etc[i] / e_sum;
				if (keys[i].nonresponse_val == 0)
				{
					// 空白パターンは固定値として更新しない
				}
				else
				{
					nonresponse_pattern_weight[keys[i].nonresponse_val] -= eta * y_etc * nonresponse_pattern_weight[keys[i].nonresponse_val];
				}
				if (keys[i].response_val != 0)
				{
					response_match_weight -= eta * y_etc * response_match_weight;
					response_pattern_weight[keys[i].response_val] -= eta * y_etc * response_pattern_weight[keys[i].response_val];
				}
				// アタリを助ける手か
				if (atari_save.bit_test(keys[i].xy))
				{
					save_atari_weight -= eta * y_etc * save_atari_weight;
				}
				// 直前の手に隣接する手か
				if (is_neighbour(board, keys[i].xy))
				{
					neighbour_weight -= eta * y_etc * neighbour_weight;
				}
			}

			learned_position_num++;
		}

		board.move(xy, color, true);
		turn++;
	}

	// 損失関数の平均値表示
	printf("%S : loss = %f\n", infile, loss / loss_cnt);

	fclose(fp);

	return 1;
}

void read_pattern()
{
	FILE* fp = fopen("response.ptn", "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "response.ptn read error\n");
		return;
	}
	while (feof(fp) == 0)
	{
		ResponsePatternVal response;
		fread(&response, sizeof(response), 1, fp);

		response_pattern_weight.insert({ response, 1.0f });
	}
	fclose(fp);

	fp = fopen("nonresponse.ptn", "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "nonresponse.ptn read error\n");
		return;
	}
	while (feof(fp) == 0)
	{
		NonResponsePatternVal nonresponse;
		fread(&nonresponse, sizeof(nonresponse), 1, fp);

		nonresponse_pattern_weight.insert({ nonresponse, 1.0f });
	}
	fclose(fp);
}

void learn_pattern(const wchar_t* dirs)
{
	int learned_game_num = 0; // 学習局数
	int learned_position_num = 0; // 学習局面数

	// 重み初期化
	save_atari_weight = 1.0f;
	neighbour_weight = 1.0f;
	response_match_weight = 1.0f;

	// パターン読み込む
	read_pattern();

	FILE *fp_dirlist = _wfopen(dirs, L"r");
	wchar_t dir[1024];
	vector<wstring> dirlist;
	while (fgetws(dir, sizeof(dir) / sizeof(dir[0]), fp_dirlist) != NULL)
	{
		wstring finddir(dir);
		finddir.pop_back();
		dirlist.push_back(finddir);
	}
	fclose(fp_dirlist);

	float iteration_eta[] = { 0.1f, 0.01f, 0.001f };
	// 棋譜を読み込んで学習
	for (float eta_tmp : iteration_eta)
	{
		eta = eta_tmp; // 学習係数
		for (auto finddir : dirlist)
		{
			// 入力ファイル一覧
			WIN32_FIND_DATA win32fd;
			HANDLE hFind = FindFirstFile((finddir + L"\\*.sgf").c_str(), &win32fd);
			if (hFind == INVALID_HANDLE_VALUE)
			{
				fprintf(stderr, "dir open error. %S\n", dir);
				return;
			}

			do {
				if (win32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					continue;
				}

				// パターン学習
				learned_game_num += learn_pattern_sgf((finddir + L"\\" + win32fd.cFileName).c_str(), learned_position_num);
			} while (FindNextFile(hFind, &win32fd));
		}
	}

	printf("response pattern weight num = %d\n", response_pattern_weight.size());
	printf("nonresponse pattern weight num = %d\n", nonresponse_pattern_weight.size());

	// 重み順にソート
	multimap<float, ResponsePatternVal> response_weight_sorted;
	multimap<float, NonResponsePatternVal> nonresponse_weight_sorted;
	for (auto itr : response_pattern_weight)
	{
		if (itr.second > 0.01f)
		{
			response_weight_sorted.insert({ itr.second, itr.first });
		}
	}
	for (auto itr : nonresponse_pattern_weight)
	{
		if (itr.second > 0.01f)
		{
			nonresponse_weight_sorted.insert({ itr.second, itr.first });
		}
	}

	printf("response pattern weight output num = %d\n", response_weight_sorted.size());
	printf("nonresponse pattern weight output num = %d\n", nonresponse_weight_sorted.size());

	printf("save atari weight = %f\n", save_atari_weight);
	printf("neighbour_weight = %f\n", neighbour_weight);
	printf("response match weight = %f\n", response_match_weight);
	// Top10
	int n = 0;
	for (auto itr = response_weight_sorted.rbegin(); itr != response_weight_sorted.rend() && n < 10; itr++, n++)
	{
		printf("response pattern weight : %llx : %f\n", itr->second, itr->first);
	}
	n = 0;
	for (auto itr = nonresponse_weight_sorted.rbegin(); itr != nonresponse_weight_sorted.rend() && n < 10; itr++, n++)
	{
		printf("nonresponse pattern weight : %llx : %f\n", itr->second, itr->first);
	}

	// 重み出力
	FILE* fp_weight = fopen("rollout.bin", "wb");
	fwrite(&save_atari_weight, sizeof(save_atari_weight), 1, fp_weight);
	fwrite(&neighbour_weight, sizeof(neighbour_weight), 1, fp_weight);
	fwrite(&response_match_weight, sizeof(response_match_weight), 1, fp_weight);
	int num = response_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = response_weight_sorted.rbegin(); itr != response_weight_sorted.rend(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	num = nonresponse_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = nonresponse_weight_sorted.rbegin(); itr != nonresponse_weight_sorted.rend(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	fclose(fp_weight);
}

int prepare_pattern_sgf(const wchar_t* infile, map<ResponsePatternVal, int>& response_pattern_map, map<NonResponsePatternVal, int>& nonresponse_pattern_map)
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
		return 0;
	}

	// 結果取得
	Color win = get_win_from_re(next, infile);
	if (win == 0)
	{
		fclose(fp);
		return 0;
	}

	Board board(19);

	int turn = 0;
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
			// 候補手一覧
			for (XY txy = BOARD_WIDTH + 1; txy < BOARD_MAX - BOARD_WIDTH; txy++)
			{
				if (board.is_empty(txy) && board.is_legal(txy, color, false) == SUCCESS)
				{
					// 候補手パターン
					// レスポンスパターン
					ResponsePatternVal response_val = response_pattern_collision0;
					response_pattern(board, txy, color, response_val);
					if (response_val != response_pattern_collision0)
					{
						response_pattern_map[response_val]++;
					}

					// ノンレスポンスパターン
					NonResponsePatternVal nonresponse_val = nonresponse_pattern_collision0;
					nonresponse_pattern(board, txy, color, nonresponse_val);
					nonresponse_pattern_map[nonresponse_val]++;
				}
			}
		}

		board.move(xy, color, true);
		turn++;
	}

	fclose(fp);

	return 1;
}

// パターン抽出と頻度調査
void prepare_pattern(const wchar_t* dirs)
{
	int learned_game_num = 0; // 学習局数

	map<ResponsePatternVal, int> response_pattern_map;
	map<NonResponsePatternVal, int> nonresponse_pattern_map;

	FILE *fp_dirlist = _wfopen(dirs, L"r");
	wchar_t dir[1024];

	// 棋譜を読み込んで学習
	while (fgetws(dir, sizeof(dir) / sizeof(dir[0]), fp_dirlist) != NULL)
	{
		// 入力ファイル一覧
		wstring finddir(dir);
		finddir.pop_back();
		WIN32_FIND_DATA win32fd;
		HANDLE hFind = FindFirstFile((finddir + L"\\*.sgf").c_str(), &win32fd);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "dir open error. %S\n", dir);
			return;
		}

		do {
			if (win32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				continue;
			}

			// パターン学習
			learned_game_num += prepare_pattern_sgf((finddir + L"\\" + win32fd.cFileName).c_str(), response_pattern_map, nonresponse_pattern_map);
		} while (FindNextFile(hFind, &win32fd));
	}
	fclose(fp_dirlist);

	printf("read game num = %d\n", learned_game_num);
	printf("response pattern num = %d\n", response_pattern_map.size());
	printf("nonresponse pattern num = %d\n", nonresponse_pattern_map.size());

	// 頻度順に並べ替え
	multimap<int, ResponsePatternVal> response_pattern_sorted;
	multimap<int, NonResponsePatternVal> nonresponse_pattern_sorted;

	for (auto itr : response_pattern_map)
	{
		auto itr2 = response_pattern_sorted.find(itr.second);
		response_pattern_sorted.insert({ itr.second, itr.first });
	}

	for (auto itr : nonresponse_pattern_map)
	{
		auto itr2 = nonresponse_pattern_sorted.find(itr.second);
		nonresponse_pattern_sorted.insert({ itr.second, itr.first });
	}

	// 頻度順に出力
	int response_pattern_outnum = 0;
	FILE* fp = fopen("response.ptn", "wb");
	for (auto itr = response_pattern_sorted.rbegin(); itr != response_pattern_sorted.rend(); itr++)
	{
		if (itr->first >= 10)
		{
			fwrite(&itr->second, sizeof(itr->second), 1, fp);
			response_pattern_outnum++;
		}
	}
	fclose(fp);

	int nonresponse_pattern_outnum = 0;
	fp = fopen("nonresponse.ptn", "wb");
	for (auto itr = nonresponse_pattern_sorted.rbegin(); itr != nonresponse_pattern_sorted.rend(); itr++)
	{
		if (itr->first >= 10)
		{
			fwrite(&itr->second, sizeof(itr->second), 1, fp);
			nonresponse_pattern_outnum++;
		}
	}
	fclose(fp);

	printf("response pattern output num = %d\n", response_pattern_outnum);
	printf("nonresponse pattern output num = %d\n", nonresponse_pattern_outnum);

	// Top10表示
	int num = 0;
	for (auto itr = response_pattern_sorted.rbegin(); itr != response_pattern_sorted.rend(); itr++)
	{
		printf("response : %llx, %d\n", itr->second.val64, itr->first);
		num++;

		if (num >= 10)
		{
			break;
		}
	}
	num = 0;
	for (auto itr = nonresponse_pattern_sorted.rbegin(); itr != nonresponse_pattern_sorted.rend(); itr++)
	{
		printf("nonresponse : %x, %d\n", itr->second.val32, itr->first);
		num++;

		if (num >= 10)
		{
			break;
		}
	}
}

void check_hash()
{
	int n = 0;
	int collision_num = 0;
	FILE* fp = fopen("response.ptn", "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "response.ptn read error\n");
		return;
	}
	while (feof(fp) == 0)
	{
		ResponsePatternVal response;
		fread(&response, sizeof(response), 1, fp);

		// ハッシュ登録
		HashKey key = get_hash_key_response_pattern(response);
		// 衝突検出
		if (response_pattern_collision[key] == 0)
		{
			response_pattern_collision[key] = response;
		}
		else if (response_pattern_collision[key] != response)
		{
			//fprintf(stderr, "response pattern collision : %d : %llx\n", n, response.val64);
			collision_num++;
		}
		n++;
	}
	fclose(fp);
	printf("response pattern collision num = %d\n", collision_num);

	n = 0;
	collision_num = 0;
	fp = fopen("nonresponse.ptn", "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "nonresponse.ptn read error\n");
		return;
	}
	while (feof(fp) == 0)
	{
		NonResponsePatternVal nonresponse;
		fread(&nonresponse, sizeof(nonresponse), 1, fp);

		// ハッシュ登録
		HashKey key = get_hash_key_nonresponse_pattern(nonresponse);
		// 衝突検出
		if (nonresponse_pattern_collision[key] == 0)
		{
			nonresponse_pattern_collision[key] = nonresponse;
		}
		else if (nonresponse_pattern_collision[key] != nonresponse)
		{
			//fprintf(stderr, "nonresponse pattern collision : %d : %x\n", n, nonresponse.val32);
			collision_num++;
		}
		n++;
	}
	fclose(fp);
	printf("nonresponse pattern collision num = %d\n", collision_num);
}

int wmain(int argc, wchar_t** argv)
{
	if (argc < 3)
	{
		return 1;
	}

	if (wcscmp(argv[1], L"prepare") == 0)
	{
		prepare_pattern(argv[2]);
	}
	else if (wcscmp(argv[1], L"learn") == 0)
	{
		learn_pattern(argv[2]);
	}
	else if (wcscmp(argv[1], L"hash") == 0)
	{
		// seed = 9999999661
		// response pattern collision num = 145
		// nonresponse pattern collision num = 11
		uint64_t seeds[] = { 9999997519llu, 9999997537llu, 9999997543llu, 9999997549llu, 9999997561llu, 9999997589llu, 9999997603llu, 9999997613llu, 9999997619llu, 9999997649llu, 9999997663llu, 9999997691llu, 9999997757llu, 9999997789llu, 9999997793llu, 9999997811llu, 9999997823llu, 9999997859llu, 9999997871llu, 9999997919llu, 9999997921llu, 9999997927llu, 9999997951llu, 9999997961llu, 9999998023llu, 9999998027llu, 9999998083llu, 9999998123llu, 9999998137llu, 9999998147llu, 9999998149llu, 9999998159llu, 9999998191llu, 9999998231llu, 9999998233llu, 9999998237llu, 9999998269llu, 9999998273llu, 9999998311llu, 9999998317llu, 9999998363llu, 9999998377llu, 9999998401llu, 9999998413llu, 9999998419llu, 9999998453llu, 9999998521llu, 9999998549llu, 9999998557llu, 9999998597llu, 9999998599llu, 9999998609llu, 9999998611llu, 9999998633llu, 9999998641llu, 9999998653llu, 9999998711llu, 9999998731llu, 9999998783llu, 9999998821llu, 9999998837llu, 9999998861llu, 9999998867llu, 9999999001llu, 9999999017llu, 9999999019llu, 9999999059llu, 9999999067llu, 9999999089llu, 9999999103llu, 9999999151llu, 9999999157llu, 9999999161llu, 9999999169llu, 9999999241llu, 9999999253llu, 9999999319llu, 9999999337llu, 9999999367llu, 9999999371llu, 9999999379llu, 9999999479llu, 9999999491llu, 9999999511llu, 9999999557llu, 9999999619llu, 9999999631llu, 9999999661llu, 9999999673llu, 9999999679llu, 9999999701llu, 9999999703llu, 9999999707llu, 9999999727llu, 9999999769llu, 9999999781llu, 9999999787llu, 9999999817llu, 9999999833llu, 9999999851llu, 9999999881llu, 9999999929llu, 9999999943llu, 9999999967llu };
		for (auto seed : seeds)
		{
			memset(response_pattern_collision, 0, sizeof(response_pattern_collision));
			memset(nonresponse_pattern_collision, 0, sizeof(nonresponse_pattern_collision));
			printf("seed = %lld\n", seed);
			init_hash_table_and_weight(seed);
			check_hash();
		}
		check_hash();
	}

	return 0;
}