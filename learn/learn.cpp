#include <windows.h>
#include <string>

#include "../Board.h"
#include "../Random.h"

using namespace std;

Random random;

typedef unsigned int PatternVal;
typedef unsigned short HashKey;

// パターン用ハッシュテーブル 12points
const int HASH_KEY_MAX_PATTERN_COLOR = 4096;
const int HASH_KEY_MAX_PATTERN_LIBERTIES = 4096 * 2;
HashKey hash_key_pattern_black[HASH_KEY_MAX_PATTERN_COLOR];
HashKey hash_key_pattern_white[HASH_KEY_MAX_PATTERN_COLOR];
HashKey hash_key_pattern_offboard[HASH_KEY_MAX_PATTERN_COLOR];
HashKey hash_key_pattern_liberties[HASH_KEY_MAX_PATTERN_LIBERTIES];
HashKey hash_key_move_pos[12];

HashKey* hash_key_pattern_color[4] = { nullptr, hash_key_pattern_black, hash_key_pattern_white, hash_key_pattern_offboard };

const HashKey HASH_TABEL_SIZE_RESPONS_PATTERN = 4096 * 5;

struct ResponsePatternVal
{
	PatternVal black;
	PatternVal white;
	PatternVal offboard;
	PatternVal liberties;
	PatternVal move_pos;
};

// レスポンスパターンの重み
float response_pattern_weight[HASH_TABEL_SIZE_RESPONS_PATTERN];

// ハッシュキー衝突検出用
ResponsePatternVal response_pattern_collision[HASH_TABEL_SIZE_RESPONS_PATTERN];

// 各色のパターン用ハッシュキー値生成
HashKey create_hash_table_pattern() {
	for (Color color : { BLACK, WHITE, OFFBOARD })
	{
		HashKey* hash_key_color = hash_key_pattern_color[color];

		for (int i = 0; i < HASH_KEY_MAX_PATTERN_COLOR; i++)
		{
			hash_key_color[i] = (HashKey)random.random();
		}

		for (int i = 0; i < HASH_KEY_MAX_PATTERN_LIBERTIES; i++)
		{
			hash_key_pattern_liberties[i] = (HashKey)random.random();
		}

		for (int i = 0; i < 12; i++)
		{
			hash_key_move_pos[i] = (HashKey)random.random();
		}
	}
}

// レスポンスパターン用ハッシュキー値取得
HashKey get_hash_key_response_pattern(const ResponsePatternVal& val)
{
	return hash_key_pattern_black[val.black] ^ hash_key_pattern_white[val.white] ^ hash_key_pattern_offboard[val.offboard] ^ hash_key_pattern_liberties[val.liberties] ^ hash_key_move_pos[val.move_pos];
}


void learn_pattern(const wchar_t** dirs, const int dir_num)
{
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
			learn_pattern_sgf((finddir + L"\\" + win32fd.cFileName).c_str());
		} while (FindNextFile(hFind, &win32fd));
	}
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

PatternVal response_pattern(const Board& board, const XY xy, Color color)
{
	// 直前の手の12ポイント範囲内か
	XY d = abs(xy - board.pre_xy);
	if (!(d <= 2 || (BOARD_WIDTH <= d && d <= BOARD_WIDTH + 1) || d == BOARD_WIDTH * 2))
	{
		return 0;
	}

	// 黒のキー値
	HashKey black;
}

void learn_pattern_sgf(const wchar_t* infile)
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

	int i = 0;
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
		if (color == win && i >= 10)
		{
			// レスポンスパターン

			unsigned int pattern = encord_pattern(board, xy, win);

			if (pattern != 0)
			{
				float e_sum = 0;
				float e_y = 0;
				float e_etc[19 * 19] = { 0 };

				ParamMap::iterator itr_y = params.end(); // 教師データと一致する手のパラメータ
				ParamMap::iterator itrs[19 * 19]; // 教師データと一致しない手のパラメータ
				int num = 0;

				// 候補手一覧
				for (XY txy = BOARD_SIZE + 3; txy < BOARD_MAX - (BOARD_SIZE + 3); txy++)
				{
					if (board[txy] == EMPTY && board.is_legal(txy, color, false) == SUCCESS)
					{
						// 候補手パターン
						unsigned int tptn = encord_pattern(board, txy, board[txy]);

						if (tptn != 0)
						{
							// 対称性を考慮してパターンを検索
							auto itr = find_pattern(params, tptn);

							if (itr != params.end())
							{

								// 各手のsoftmaxを計算
								float e = expf(itr->second);
								e_sum += e;

								// 教師データと一致する場合
								if (txy == xy)
								{
									e_y = e;
									itr_y = itr;
								}
								else {
									e_etc[num] = e;
									itrs[num] = itr;
									num++;
								}
							}
						}
					}
				}

				if (itr_y != params.end())
				{
					// 教師データと一致する手のsoftmax
					float y = e_y / e_sum;

					// 教師データと一致する手のパラメータ更新
					itr_y->second -= learning_rate * (y - 1.0f) * itr_y->second;

					// 損失関数
					loss += -logf(y);
					loss_cnt++;
				}

				// 教師データと一致しない手のパラメータ更新
				for (int i = 0; i < num; i++)
				{
					float y_etc = e_etc[i] / e_sum;
					auto itr = itrs[i];
					itr->second -= learning_rate * y_etc * itr->second;
				}

				position_num++;
			}
		}

		board.move(xy, color, true);
		i++;
	}

	// 損失関数の平均値表示
	printf("loss = %f\n", loss / loss_cnt);

	fclose(fp);
	game_num++;
}