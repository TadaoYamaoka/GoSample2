#include <windows.h>
#include <string>
#include <cassert>
#include <vector>

#include "../Board.h"
#include "../Random.h"
#include "Pattern.h"
#include "Sgf.h"

using namespace std;

// 学習係数
float eta = 0.01;

// 正則化係数
float ramda = 0.00000001;

// パターン用ハッシュテーブル 12points
const int HASH_KEY_MAX_PATTERN = 12 / 2; // color,libertiesのセット4byte * 2単位
HashKey hash_key_pattern[HASH_KEY_MAX_PATTERN][256];
HashKey hash_key_move_pos[5 * 5];

const int HASH_KEY_BIT = 24;
const int HASH_KEY_MAX = 1 << HASH_KEY_BIT;
const int HASH_KEY_MASK = HASH_KEY_MAX - 1;

// rollout policyの重み
RolloutPolicyWeight rpw;

// tree policyの重み
TreePolicyWeight tpw;

// ハッシュキー衝突検出用
ResponsePatternVal response_pattern_collision[HASH_KEY_MAX];
NonResponsePatternVal nonresponse_pattern_collision[HASH_KEY_MAX];

template <typename T>
inline T sgn(const T val)
{
	if (val > 0)
	{
		return 1;
	}
	else if (val < 0)
	{
		return -1;
	}
	return 0;
}

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

// ノンレスポンスパターン用ハッシュキー値取得
HashKey get_hash_key_nonresponse_pattern(const NonResponsePatternVal& val)
{
	return hash_key_pattern[0][val.vals.color_liberties[0]]
		^ hash_key_pattern[1][val.vals.color_liberties[1]]
		^ hash_key_pattern[2][val.vals.color_liberties[2]]
		^ hash_key_pattern[3][val.vals.color_liberties[3]];
}

// 文字列検索(UTF-8)
const char* strsearch(const char* str, const int len, const char* search, const int searchlen)
{
	// BM法で検索
	for (int pos = searchlen - 1; pos < len; )
	{
		bool match = true;
		int i;
		int j;
		for (i = 0, j = 0; i < searchlen && pos + i < len; i++)
		{
			if (str[pos - j] == search[searchlen - i - 1])
			{
				if (!match)
				{
					break;
				}
				match = true;
				j++;
			}
			else
			{
				match = false;
			}
		}

		if (match)
		{
			return str + pos - searchlen + 1;
		}
		else
		{
			pos += i;
		}
	}
	return nullptr;
}

// 文字検索(UTF-8)
const char* strsearch_char(const char* str, const int len, const char c)
{
	for (int i = 0; i < len; i++)
	{
		if (str[i] == c)
		{
			return str + i;
		}
	}
	return nullptr;
}

bool is_exclude(char* next)
{
	const char* ev = strsearch(next, 200, "EV[", 3);


	if (ev == nullptr)
	{
		return false;
	}

	const char* ev_end = strsearch_char(ev, 200, ']');
	if (ev_end == nullptr)
	{
		return false;
	}
	const int ev_len = ev_end - ev;

	// 指導(UTF-8)
	const char sido[] = { 0xe6, 0x8c, 0x87, 0xe5, 0xb0, 0x8e };
	// アマ(UTF-8)
	const char ama[] = { 0xe3, 0x82, 0xa2, 0xe3, 0x83, 0x9e };

	if (strsearch(ev, ev_len, sido, sizeof(sido)))
	{
		return true;
	}
	else if (strsearch(ev, ev_len, ama, sizeof(ama)))
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

// 教師データに一致する手の更新量
inline float delta_weight_y(float y, float weight)
{
	return eta * (y - 1.0f) * weight + sgn(weight) * ramda;
}

// 教師データに一致しない手の更新量
inline float delta_weight_y_etc(float y_etc, float weight)
{
	return eta * y_etc * weight + sgn(weight) * ramda;
}

// 教師データに一致する手の更新
template <typename T, typename K>
inline void update_weight_y(T& weightmap, const K& key, float y)
{
	auto itr = weightmap.find(key);
	if (itr == weightmap.end())
	{
		return;
	}
	itr->second -= delta_weight_y(y, itr->second);
}

// 教師データに一致しない手の更新
template <typename T, typename K>
inline void update_weight_y_etc(T& weightmap, const K& key, float y_etc)
{
	auto itr = weightmap.find(key);
	if (itr == weightmap.end())
	{
		return;
	}
	itr->second -= delta_weight_y_etc(y_etc, itr->second);
}

template <typename T, typename K>
inline float get_weight_val(T& weightmap, const K& key)
{
	auto itr = weightmap.find(key);
	if (itr == weightmap.end())
	{
		return 0;
	}
	return itr->second;
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
	if (is_exclude(next))
	{
		fclose(fp);
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
	float rollout_loss = 0;
	int rollout_loss_cnt = 0;
	float tree_loss = 0;
	int tree_loss_cnt = 0;
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
			float rollout_e_sum = 0;
			float rollout_e_y = 0;
			float rollout_e_etc[19 * 19] = { 0 };

			float tree_e_sum = 0;
			float tree_e_y = 0;
			float tree_e_etc[19 * 19] = { 0 };

			struct Key
			{
				XY xy;
				ResponsePatternVal response_val;
				NonResponsePatternVal nonresponse_val;
				Diamond12PatternVal diamond12_val;
				bool is_save_atari;
				bool is_self_atari;
				bool is_neighbour;
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
					ResponsePatternVal response_val = response_pattern(board, txy, color);

					// ノンレスポンスパターン
					NonResponsePatternVal nonresponse_val = nonresponse_pattern(board, txy, color);

					// Diamond12パターン
					Diamond12PatternVal diamond12_val = diamond12_pattern(board, txy, color);

					bool is_save_atari_tmp = false;
					bool is_self_atari_tmp = false;
					bool is_neighbour_tmp = false;

					// パラメータ更新準備
					// rollout policy
					// 重みの線形和
					float rollout_weight_sum = get_weight_val(rpw.nonresponse_pattern_weight, nonresponse_val);
					if (response_val != 0)
					{
						rollout_weight_sum += rpw.response_match_weight;
						rollout_weight_sum += get_weight_val(rpw.response_pattern_weight, response_val);
					}
					// アタリを助ける手か
					if (atari_save.bit_test(txy))
					{
						rollout_weight_sum += rpw.save_atari_weight;
					}
					// 直前の手に隣接する手か
					if (is_neighbour(board, txy))
					{
						rollout_weight_sum += rpw.neighbour_weight;
					}

					// 各手のsoftmaxを計算
					float rollout_e = expf(rollout_weight_sum);
					rollout_e_sum += rollout_e;


					// tree policy
					// 重みの線形和
					float tree_weight_sum = get_weight_val(tpw.nonresponse_pattern_weight, nonresponse_val);
					if (response_val != 0)
					{
						tree_weight_sum += tpw.response_match_weight;
						tree_weight_sum += get_weight_val(tpw.response_pattern_weight, response_val);
					}
					// アタリを助ける手か
					if (atari_save.bit_test(txy))
					{
						tree_weight_sum += tpw.save_atari_weight;
						is_save_atari_tmp = true;
					}
					// 直前の手に隣接する手か
					if (is_neighbour(board, txy))
					{
						tree_weight_sum += tpw.neighbour_weight;
						is_neighbour_tmp = true;
					}
					// アタリになる手か
					if (board.is_self_atari(color, txy))
					{
						is_self_atari_tmp = true;
					}
					else
					{
						// ならない方を評価
						tree_weight_sum += tpw.self_atari_weight;
					}
					// 直前2手からの距離
					if (board.pre_xy[0] != PASS)
					{
						XY distance = get_distance(txy, board.pre_xy[0]);
						if (distance < sizeof(tpw.last_move_distance_weight[0]) / sizeof(tpw.last_move_distance_weight[0][0]))
						{
							tree_weight_sum += tpw.last_move_distance_weight[0][distance];
						}
					}
					if (board.pre_xy[1] != PASS)
					{
						XY distance = get_distance(txy, board.pre_xy[1]);
						if (distance < sizeof(tpw.last_move_distance_weight[1]) / sizeof(tpw.last_move_distance_weight[1][0]))
						{
							tree_weight_sum += tpw.last_move_distance_weight[1][distance];
						}
					}
					// 12-point diamondパターン
					tree_weight_sum += get_weight_val(tpw.diamond12_pattern_weight, diamond12_val);

					// 各手のsoftmaxを計算
					float tree_e = expf(tree_weight_sum);
					tree_e_sum += tree_e;


					// 教師データと一致する場合
					if (txy == xy)
					{
						rollout_e_y = rollout_e;
						tree_e_y = tree_e;
						key_y.response_val = response_val;
						key_y.nonresponse_val = nonresponse_val;
						key_y.diamond12_val = diamond12_val;
						key_y.is_save_atari = is_save_atari_tmp;
						key_y.is_self_atari = is_self_atari_tmp;
						key_y.is_neighbour = is_neighbour_tmp;
					}
					else {
						rollout_e_etc[num] = rollout_e;
						tree_e_etc[num] = tree_e;
						keys[num].xy = txy;
						keys[num].response_val = response_val;
						keys[num].nonresponse_val = nonresponse_val;
						keys[num].diamond12_val = diamond12_val;
						keys[num].is_save_atari = is_save_atari_tmp;
						keys[num].is_self_atari = is_self_atari_tmp;
						keys[num].is_neighbour = is_neighbour_tmp;
						num++;
					}
				}
			}

			// 教師データと一致する手のsoftmax
			float rollout_y = rollout_e_y / rollout_e_sum;
			float tree_y = tree_e_y / tree_e_sum;

			// 教師データと一致する手のパラメータ更新
			if (key_y.nonresponse_val != 0) // 空白パターンは制約条件として更新しない
			{
				// rollout policy
				update_weight_y(rpw.nonresponse_pattern_weight, key_y.nonresponse_val, rollout_y);
				// tree policy
				update_weight_y(tpw.nonresponse_pattern_weight, key_y.nonresponse_val, tree_y);
			}
			if (key_y.response_val != 0)
			{
				// rollout policy
				rpw.response_match_weight -= delta_weight_y(rollout_y, rpw.response_match_weight);
				update_weight_y(rpw.response_pattern_weight, key_y.response_val, rollout_y);
				// tree policy
				tpw.response_match_weight -= delta_weight_y(tree_y, tpw.response_match_weight);
				update_weight_y(tpw.response_pattern_weight, key_y.response_val, tree_y);
			}
			// アタリを助ける手か
			if (key_y.is_save_atari)
			{
				// rollout policy
				rpw.save_atari_weight -= delta_weight_y(rollout_y, rpw.save_atari_weight);
				// tree policy
				tpw.save_atari_weight -= delta_weight_y(tree_y, tpw.save_atari_weight);
			}
			// 直前の手に隣接する手か
			if (key_y.is_neighbour)
			{
				// rollout policy
				rpw.neighbour_weight -= delta_weight_y(rollout_y, rpw.neighbour_weight);
				// tree policy
				tpw.neighbour_weight -= delta_weight_y(tree_y, tpw.neighbour_weight);
			}
			// アタリになる手か(ならない方を評価)
			if (!key_y.is_self_atari)
			{
				// tree policy
				tpw.self_atari_weight -= delta_weight_y(tree_y, tpw.self_atari_weight);
			}
			// 直前の2手からの距離
			for (int move = 0; move < 2; move++)
			{
				if (board.pre_xy[move] != PASS)
				{
					XY distance = get_distance(xy, board.pre_xy[move]);
					/*if (distance == 0 && move == 0)
					{
						printf("turn = %d : move = %s, (%d, %d), (%d, %d)\n", turn, next, get_x(xy), get_y(xy), get_x(board.pre_xy[move]), get_y(board.pre_xy[move]));
						exit(0);
					}*/
					if (distance < sizeof(tpw.last_move_distance_weight[0]) / sizeof(tpw.last_move_distance_weight[0][0]))
					{
						tpw.last_move_distance_weight[move][distance] -= delta_weight_y(tree_y, tpw.last_move_distance_weight[move][distance]);
					}
				}
			}
			// 12-point diamondパターン
			update_weight_y(tpw.diamond12_pattern_weight, key_y.diamond12_val, tree_y);

			// 損失関数
			rollout_loss += -logf(rollout_y);
			rollout_loss_cnt++;
			tree_loss += -logf(tree_y);
			tree_loss_cnt++;

			// 教師データと一致しない手のパラメータ更新
			for (int i = 0; i < num; i++)
			{
				float rollout_y_etc = rollout_e_etc[i] / rollout_e_sum;
				float tree_y_etc = tree_e_etc[i] / tree_e_sum;
				if (keys[i].nonresponse_val != 0) // 空白パターンは制約条件として更新しない
				{
					// rollout policy
					update_weight_y_etc(rpw.nonresponse_pattern_weight, keys[i].nonresponse_val, rollout_y_etc);
					// tree policy
					update_weight_y_etc(tpw.nonresponse_pattern_weight, keys[i].nonresponse_val, tree_y_etc);
				}
				if (keys[i].response_val != 0)
				{
					// rollout policy
					rpw.response_match_weight -= delta_weight_y_etc(rollout_y_etc, rpw.response_match_weight);
					update_weight_y_etc(rpw.response_pattern_weight, keys[i].response_val, rollout_y_etc);
					// tree policy
					tpw.response_match_weight -= delta_weight_y_etc(tree_y_etc, tpw.response_match_weight);
					update_weight_y_etc(tpw.response_pattern_weight, keys[i].response_val, tree_y_etc);
				}
				// アタリを助ける手か
				if (keys[i].is_save_atari)
				{
					// rollout policy
					rpw.save_atari_weight -= delta_weight_y_etc(rollout_y_etc, rpw.save_atari_weight);
					// tree policy
					tpw.save_atari_weight -= delta_weight_y_etc(tree_y_etc, tpw.save_atari_weight);
				}
				// 直前の手に隣接する手か
				if (keys[i].is_neighbour)
				{
					// rollout policy
					rpw.neighbour_weight -= delta_weight_y_etc(rollout_y_etc, rpw.neighbour_weight);
					// tree policy
					tpw.neighbour_weight -= delta_weight_y_etc(tree_y_etc, tpw.neighbour_weight);
				}
				// アタリになる手か(ならない方を評価)
				if (!keys[i].is_self_atari)
				{
					// tree policy
					tpw.self_atari_weight -= delta_weight_y_etc(tree_y_etc, tpw.self_atari_weight);
				}
				// 直前の2手からの距離
				for (int move = 0; move < 2; move++)
				{
					if (board.pre_xy[move] != PASS)
					{
						XY distance = get_distance(keys[i].xy, board.pre_xy[move]);
						/*if (distance == 0 && move == 0)
						{
							printf("turn = %d : move = %s, (%d, %d), (%d, %d)\n", turn, next, get_x(xy), get_y(xy), get_x(board.pre_xy[move]), get_y(board.pre_xy[move]));
							exit(0);
						}*/
						if (distance < sizeof(tpw.last_move_distance_weight[0]) / sizeof(tpw.last_move_distance_weight[0][0]))
						{
							tpw.last_move_distance_weight[move][distance] -= delta_weight_y_etc(tree_y_etc, tpw.last_move_distance_weight[move][distance]);
						}
					}
				}
				// 12-point diamondパターン
				update_weight_y_etc(tpw.diamond12_pattern_weight, keys[i].diamond12_val, tree_y_etc);
			}

			learned_position_num++;
		}

		board.move(xy, color, true);
		turn++;
	}

	// 損失関数の平均値表示
	printf("%S : rollout loss = %.5f, tree loss = %.5f\n", infile, rollout_loss / rollout_loss_cnt, tree_loss / tree_loss_cnt);

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

		rpw.response_pattern_weight.insert({ response, 1.0f });
		tpw.response_pattern_weight.insert({ response, 1.0f });
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

		rpw.nonresponse_pattern_weight.insert({ nonresponse, 1.0f });
		tpw.nonresponse_pattern_weight.insert({ nonresponse, 1.0f });
	}
	fclose(fp);

	fp = fopen("diamond12.ptn", "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "diamond12.ptn read error\n");
		return;
	}
	while (feof(fp) == 0)
	{
		Diamond12PatternVal diamond12;
		fread(&diamond12, sizeof(diamond12), 1, fp);

		tpw.diamond12_pattern_weight.insert({ diamond12, 1.0f });
	}
	fclose(fp);
}

void learn_pattern(const wchar_t* dirs)
{
	int learned_game_num = 0; // 学習局数
	int learned_position_num = 0; // 学習局面数

	// 重み初期化
	rpw.save_atari_weight = 1.0f;
	rpw.neighbour_weight = 1.0f;
	rpw.response_match_weight = 1.0f;

	tpw.save_atari_weight = 1.0f;
	tpw.neighbour_weight = 1.0f;
	tpw.response_match_weight = 1.0f;
	tpw.self_atari_weight = 1.0f;
	for (int move = 0; move < 2; move++)
	{
		for (int i = 0; i < sizeof(tpw.last_move_distance_weight[0]) / sizeof(tpw.last_move_distance_weight[0][0]); i++)
		{
			tpw.last_move_distance_weight[move][i] = 1.0f;
		}
	}

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

	float iteration_eta[] = { 0.01f, 0.001f };
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

	// 学習結果表示
	// rollout policy
	printf("rollout policy response pattern weight num = %d\n", rpw.response_pattern_weight.size());
	printf("rollout policy nonresponse pattern weight num = %d\n", rpw.nonresponse_pattern_weight.size());

	// 重み順にソート
	multimap<float, ResponsePatternVal> rpw_response_weight_sorted;
	multimap<float, NonResponsePatternVal> rpw_nonresponse_weight_sorted;
	for (auto itr : rpw.response_pattern_weight)
	{
		if (itr.second > 0.01f)
		{
			rpw_response_weight_sorted.insert({ itr.second, itr.first });
		}
	}
	for (auto itr : rpw.nonresponse_pattern_weight)
	{
		if (itr.second > 0.01f)
		{
			rpw_nonresponse_weight_sorted.insert({ itr.second, itr.first });
		}
	}

	printf("rollout policy response pattern weight output num = %d\n", rpw_response_weight_sorted.size());
	printf("rollout policy nonresponse pattern weight output num = %d\n", rpw_nonresponse_weight_sorted.size());

	printf("rollout policy save atari weight = %f\n", rpw.save_atari_weight);
	printf("rollout policy neighbour weight = %f\n", rpw.neighbour_weight);
	printf("rollout policy response match weight = %f\n", rpw.response_match_weight);
	// Top10
	int n = 0;
	for (auto itr = rpw_response_weight_sorted.rbegin(); itr != rpw_response_weight_sorted.rend() && n < 10; itr++, n++)
	{
		printf("rollout policy response pattern weight : %llx : %f\n", itr->second, itr->first);
	}
	n = 0;
	for (auto itr = rpw_nonresponse_weight_sorted.rbegin(); itr != rpw_nonresponse_weight_sorted.rend() && n < 10; itr++, n++)
	{
		printf("rollout policy nonresponse pattern weight : %llx : %f\n", itr->second, itr->first);
	}

	// tree policy
	printf("tree policy response pattern weight num = %d\n", tpw.response_pattern_weight.size());
	printf("tree policy nonresponse pattern weight num = %d\n", tpw.nonresponse_pattern_weight.size());
	printf("tree policy diamond12 pattern weight num = %d\n", tpw.diamond12_pattern_weight.size());

	multimap<float, ResponsePatternVal> tpw_response_weight_sorted;
	multimap<float, NonResponsePatternVal> tpw_nonresponse_weight_sorted;
	multimap<float, Diamond12PatternVal> tpw_diamond12_weight_sorted;
	for (auto itr : tpw.response_pattern_weight)
	{
		if (itr.second > 0.01f)
		{
			tpw_response_weight_sorted.insert({ itr.second, itr.first });
		}
	}
	for (auto itr : tpw.nonresponse_pattern_weight)
	{
		if (itr.second > 0.01f)
		{
			tpw_nonresponse_weight_sorted.insert({ itr.second, itr.first });
		}
	}
	for (auto itr : tpw.diamond12_pattern_weight)
	{
		if (itr.second > 0.01f)
		{
			tpw_diamond12_weight_sorted.insert({ itr.second, itr.first });
		}
	}

	printf("tree policy response pattern weight output num = %d\n", tpw_response_weight_sorted.size());
	printf("tree policy nonresponse pattern weight output num = %d\n", tpw_nonresponse_weight_sorted.size());
	printf("tree policy diamond12 pattern weight output num = %d\n", tpw_diamond12_weight_sorted.size());

	printf("tree policy save atari weight = %f\n", tpw.save_atari_weight);
	printf("tree policy neighbour weight = %f\n", tpw.neighbour_weight);
	printf("tree policy response match weight = %f\n", tpw.response_match_weight);
	printf("tree policy self atari weight = %f\n", tpw.self_atari_weight);
	for (int move = 0; move < 2; move++)
	{
		printf("tree policy last move distance %d = ", move);
		for (int i = 0; i < sizeof(tpw.last_move_distance_weight[0]) / sizeof(tpw.last_move_distance_weight[0][0]); i++)
		{
			printf("%f ", tpw.last_move_distance_weight[move][i]);
		}
		printf("\n");
	}
	// Top10
	n = 0;
	for (auto itr = tpw_response_weight_sorted.rbegin(); itr != tpw_response_weight_sorted.rend() && n < 10; itr++, n++)
	{
		printf("tree policy response pattern weight : %llx : %f\n", itr->second, itr->first);
	}
	n = 0;
	for (auto itr = tpw_nonresponse_weight_sorted.rbegin(); itr != tpw_nonresponse_weight_sorted.rend() && n < 10; itr++, n++)
	{
		printf("tree policy nonresponse pattern weight : %lx : %f\n", itr->second, itr->first);
	}
	n = 0;
	for (auto itr = tpw_diamond12_weight_sorted.rbegin(); itr != tpw_diamond12_weight_sorted.rend() && n < 10; itr++, n++)
	{
		printf("tree policy diamond12 pattern weight : %llx : %f\n", itr->second, itr->first);
	}

	// 重み出力
	// rollout policy
	FILE* fp_weight = fopen("rollout.bin", "wb");
	fwrite(&rpw.save_atari_weight, sizeof(rpw.save_atari_weight), 1, fp_weight);
	fwrite(&rpw.neighbour_weight, sizeof(rpw.neighbour_weight), 1, fp_weight);
	fwrite(&rpw.response_match_weight, sizeof(rpw.response_match_weight), 1, fp_weight);
	int num = rpw_response_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = rpw_response_weight_sorted.rbegin(); itr != rpw_response_weight_sorted.rend(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	num = rpw_nonresponse_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = rpw_nonresponse_weight_sorted.rbegin(); itr != rpw_nonresponse_weight_sorted.rend(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	fclose(fp_weight);

	// tree policy
	fp_weight = fopen("tree.bin", "wb");
	fwrite(&tpw.save_atari_weight, sizeof(tpw.save_atari_weight), 1, fp_weight);
	fwrite(&tpw.neighbour_weight, sizeof(tpw.neighbour_weight), 1, fp_weight);
	fwrite(&tpw.response_match_weight, sizeof(tpw.response_match_weight), 1, fp_weight);
	fwrite(&tpw.self_atari_weight, sizeof(tpw.self_atari_weight), 1, fp_weight);
	fwrite(&tpw.last_move_distance_weight, sizeof(tpw.last_move_distance_weight), 1, fp_weight);
	num = tpw_response_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw_response_weight_sorted.rbegin(); itr != tpw_response_weight_sorted.rend(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	num = tpw_nonresponse_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw_nonresponse_weight_sorted.rbegin(); itr != tpw_nonresponse_weight_sorted.rend(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	num = tpw_diamond12_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw_diamond12_weight_sorted.rbegin(); itr != tpw_diamond12_weight_sorted.rend(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	fclose(fp_weight);
}

int prepare_pattern_sgf(const wchar_t* infile, map<ResponsePatternVal, int>& response_pattern_map, map<NonResponsePatternVal, int>& nonresponse_pattern_map, map<Diamond12PatternVal, int>& diamond12_pattern_map)
{
	FILE* fp = _wfopen(infile, L"r");
	char buf[10000];
	// 1行目読み飛ばし
	fgets(buf, sizeof(buf), fp);
	// 2行目
	fgets(buf, sizeof(buf), fp);

	// ;で区切る
	char* next = strtok(buf, ";");

	// 指導碁、アマ除外
	if (is_exclude(next))
	{
		fclose(fp);
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
					ResponsePatternVal response_val = response_pattern(board, txy, color);
					if (response_val != 0)
					{
						response_pattern_map[response_val]++;
					}

					// ノンレスポンスパターン
					NonResponsePatternVal nonresponse_val = nonresponse_pattern(board, txy, color);
					nonresponse_pattern_map[nonresponse_val]++;

					// 12-point diamondパターン
					Diamond12PatternVal diamond12_val = diamond12_pattern(board, txy, color);
					diamond12_pattern_map[diamond12_val]++;
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
	map<Diamond12PatternVal, int> diamond12_pattern_map;

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

			// パターン抽出
			learned_game_num += prepare_pattern_sgf((finddir + L"\\" + win32fd.cFileName).c_str(), response_pattern_map, nonresponse_pattern_map, diamond12_pattern_map);
		} while (FindNextFile(hFind, &win32fd));
	}
	fclose(fp_dirlist);

	printf("read game num = %d\n", learned_game_num);
	printf("response pattern num = %d\n", response_pattern_map.size());
	printf("nonresponse pattern num = %d\n", nonresponse_pattern_map.size());
	printf("diamond12 pattern num = %d\n", diamond12_pattern_map.size());

	// 頻度順に並べ替え
	multimap<int, ResponsePatternVal> response_pattern_sorted;
	multimap<int, NonResponsePatternVal> nonresponse_pattern_sorted;
	multimap<int, Diamond12PatternVal> diamond12_pattern_sorted;

	int response_pattern_sum = 0;
	for (auto itr : response_pattern_map)
	{
		auto itr2 = response_pattern_sorted.find(itr.second);
		response_pattern_sorted.insert({ itr.second, itr.first });
		response_pattern_sum += itr.second;
	}
	printf("response pattern sum = %d\n", response_pattern_sum);

	int nonresponse_pattern_sum = 0;
	for (auto itr : nonresponse_pattern_map)
	{
		auto itr2 = nonresponse_pattern_sorted.find(itr.second);
		nonresponse_pattern_sorted.insert({ itr.second, itr.first });
		nonresponse_pattern_sum += itr.second;
	}
	printf("nonresponse pattern sum = %d\n", nonresponse_pattern_sum);

	int diamond12_pattern_sum = 0;
	for (auto itr : diamond12_pattern_map)
	{
		auto itr2 = diamond12_pattern_sorted.find(itr.second);
		diamond12_pattern_sorted.insert({ itr.second, itr.first });
		diamond12_pattern_sum += itr.second;
	}
	printf("diamond12 pattern sum = %d\n", diamond12_pattern_sum);

	// 頻度順に出力
	int response_pattern_outnum = 0;
	FILE* fp = fopen("response.ptn", "wb");
	for (auto itr = response_pattern_sorted.rbegin(); itr != response_pattern_sorted.rend(); itr++)
	{
		if (itr->first < 50)
		{
			break;
		}
		fwrite(&itr->second, sizeof(itr->second), 1, fp);
		response_pattern_outnum++;
	}
	fclose(fp);

	int nonresponse_pattern_outnum = 0;
	fp = fopen("nonresponse.ptn", "wb");
	for (auto itr = nonresponse_pattern_sorted.rbegin(); itr != nonresponse_pattern_sorted.rend(); itr++)
	{
		if (itr->first < 10)
		{
			break;
		}
		fwrite(&itr->second, sizeof(itr->second), 1, fp);
		nonresponse_pattern_outnum++;
	}

	int diamond12_pattern_outnum = 0;
	fp = fopen("diamond12.ptn", "wb");
	for (auto itr = diamond12_pattern_sorted.rbegin(); itr != diamond12_pattern_sorted.rend(); itr++)
	{
		if (itr->first < 500)
		{
			break;
		}
		fwrite(&itr->second, sizeof(itr->second), 1, fp);
		diamond12_pattern_outnum++;
	}
	fclose(fp);

	printf("response pattern output num = %d\n", response_pattern_outnum);
	printf("nonresponse pattern output num = %d\n", nonresponse_pattern_outnum);
	printf("diamond12 pattern output num = %d\n", diamond12_pattern_outnum);

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
	num = 0;
	for (auto itr = diamond12_pattern_sorted.rbegin(); itr != diamond12_pattern_sorted.rend(); itr++)
	{
		printf("diamond12 : %x, %d\n", itr->second.val64, itr->first);
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

inline void print_response_pattern_stone(const ResponsePatternVal& val, const int n)
{
	const int move_idx[] = {
		0, 0, 1, 0, 0,
		0, 2, 3, 4, 0,
		5, 6, 0, 7, 8,
		0, 9, 10, 11, 0,
		0, 0, 12, 0, 0
	};
	if (move_idx[val.vals.move_pos] == n)
	{
		printf("| × ");
	}
	else
	{
		Color color = (val.val64 >> ((n - 1) * 4)) & 0b11ull;
		int liberty_num = (val.val64 >> ((n - 1) * 4 + 2)) & 0b11ull;
		printf("| %s%d", color == BLACK ? "●" : "○", liberty_num);
	}
}

void print_response_pattern(const ResponsePatternVal& val)
{
	// 1段目
	printf("| 　 | 　 ");
	print_response_pattern_stone(val, 1);
	printf("| 　 | 　 |\n");
	// 2段目
	printf("| 　 ");
	print_response_pattern_stone(val, 2);
	print_response_pattern_stone(val, 3);
	print_response_pattern_stone(val, 4);
	printf("| 　 |\n");
	// 3段目
	print_response_pattern_stone(val, 5);
	print_response_pattern_stone(val, 6);
	printf("| ◎ ");
	print_response_pattern_stone(val, 7);
	print_response_pattern_stone(val, 8);
	printf("|\n");
	// 4段目
	printf("| 　 ");
	print_response_pattern_stone(val, 9);
	print_response_pattern_stone(val, 10);
	print_response_pattern_stone(val, 11);
	printf("| 　 |\n");
	// 5段目
	printf("| 　 | 　 ");
	print_response_pattern_stone(val, 12);
	printf("| 　 | 　 |\n");
}

inline void print_nonresponse_pattern_stone(const NonResponsePatternVal& val, const int n)
{
	Color color = (val.val32 >> ((n - 1) * 4)) & 0b11ull;
	int liberty_num = (val.val32 >> ((n - 1) * 4 + 2)) & 0b11ull;
	printf("| %s%d", color == BLACK ? "●" : "○", liberty_num);
}

void print_nonresponse_pattern(const NonResponsePatternVal& val)
{
	// 2段目
	print_nonresponse_pattern_stone(val, 1);
	print_nonresponse_pattern_stone(val, 2);
	print_nonresponse_pattern_stone(val, 3);
	printf("|\n");
	// 3段目
	print_nonresponse_pattern_stone(val, 4);
	printf("| × ");
	print_nonresponse_pattern_stone(val, 5);
	printf("|\n");
	// 4段目
	print_nonresponse_pattern_stone(val, 6);
	print_nonresponse_pattern_stone(val, 7);
	print_nonresponse_pattern_stone(val, 8);
	printf("|\n");
}

inline void print_diamond12_pattern_stone(const Diamond12PatternVal& val, const int n)
{
	Color color = (val.val64 >> ((n - 1) * 4))& 0b11ull;
	int liberty_num = (val.val64 >> ((n - 1) * 4 + 2)) & 0b11ull;
	printf("| %s%d", color == BLACK ? "●" : "○", liberty_num);
}

void print_diamond12_pattern(const Diamond12PatternVal& val)
{
	// 1段目
	printf("| 　 | 　 ");
	print_diamond12_pattern_stone(val, 1);
	printf("| 　 | 　 |\n");
	// 2段目
	printf("| 　 ");
	print_diamond12_pattern_stone(val, 2);
	print_diamond12_pattern_stone(val, 3);
	print_diamond12_pattern_stone(val, 4);
	printf("| 　 |\n");
	// 3段目
	print_diamond12_pattern_stone(val, 5);
	print_diamond12_pattern_stone(val, 6);
	printf("| × ");
	print_diamond12_pattern_stone(val, 7);
	print_diamond12_pattern_stone(val, 8);
	printf("|\n");
	// 4段目
	printf("| 　 ");
	print_diamond12_pattern_stone(val, 9);
	print_diamond12_pattern_stone(val, 10);
	print_diamond12_pattern_stone(val, 11);
	printf("| 　 |\n");
	// 5段目
	printf("| 　 | 　 ");
	print_diamond12_pattern_stone(val, 12);
	printf("| 　 | 　 |\n");
}

void dump_weight()
{
	// 重み順にソート
	multimap<float, ResponsePatternVal> rollout_response_weight_sorted;
	multimap<float, NonResponsePatternVal> rollout_nonresponse_weight_sorted;
	multimap<float, ResponsePatternVal> tree_response_weight_sorted;
	multimap<float, NonResponsePatternVal> tree_nonresponse_weight_sorted;
	multimap<float, Diamond12PatternVal> tree_diamond12_weight_sorted;

	// 重み読み込み
	// rollout policy
	FILE* fp_weight = fopen("rollout.bin", "rb");
	fread(&rpw.save_atari_weight, sizeof(rpw.save_atari_weight), 1, fp_weight);
	fread(&rpw.neighbour_weight, sizeof(rpw.neighbour_weight), 1, fp_weight);
	fread(&rpw.response_match_weight, sizeof(rpw.response_match_weight), 1, fp_weight);
	int num;
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		ResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		rollout_response_weight_sorted.insert({ weight, val });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		NonResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		rollout_nonresponse_weight_sorted.insert({ weight, val });
	}
	fclose(fp_weight);

	// 表示
	printf("rollout response pattern weight output num = %d\n", rollout_response_weight_sorted.size());
	printf("rollout nonresponse pattern weight output num = %d\n", rollout_nonresponse_weight_sorted.size());

	printf("rollout save atari weight = %f\n", rpw.save_atari_weight);
	printf("rollout neighbour_weight = %f\n", rpw.neighbour_weight);
	printf("rollout response match weight = %f\n", rpw.response_match_weight);

	// Top10
	printf("Top10\n");
	int n = 0;
	for (auto itr = rollout_response_weight_sorted.rbegin(); itr != rollout_response_weight_sorted.rend() && n < 10; itr++, n++)
	{
		if (n == 0)
		{
			print_response_pattern(itr->second);
			printf("rotate\n");
			print_response_pattern(itr->second.rotate());
			printf("vmirror\n");
			print_response_pattern(itr->second.vmirror());
			printf("hmirror\n");
			print_response_pattern(itr->second.hmirror());
		}
		printf("rollout response pattern weight : %llx : %f\n", itr->second, itr->first);
	}
	n = 0;
	for (auto itr = rollout_nonresponse_weight_sorted.rbegin(); itr != rollout_nonresponse_weight_sorted.rend() && n < 10; itr++, n++)
	{
		if (n == 0)
		{
			print_nonresponse_pattern(itr->second);
			printf("rotate\n");
			print_nonresponse_pattern(itr->second.rotate());
			printf("vmirror\n");
			print_nonresponse_pattern(itr->second.vmirror());
			printf("hmirror\n");
			print_nonresponse_pattern(itr->second.hmirror());
		}
		printf("rollout nonresponse pattern weight : %lx : %f\n", itr->second, itr->first);
	}

	// Bottom10
	printf("Bottom10\n");
	n = 0;
	for (auto itr = rollout_response_weight_sorted.begin(); itr != rollout_response_weight_sorted.end() && n < 10; itr++, n++)
	{
		printf("rollout response pattern weight : %llx : %f\n", itr->second, itr->first);
	}
	n = 0;
	for (auto itr = rollout_nonresponse_weight_sorted.begin(); itr != rollout_nonresponse_weight_sorted.end() && n < 10; itr++, n++)
	{
		printf("rollout nonresponse pattern weight : %lx : %f\n", itr->second, itr->first);
	}

	// tree policy
	fp_weight = fopen("tree.bin", "rb");
	fread(&tpw.save_atari_weight, sizeof(tpw.save_atari_weight), 1, fp_weight);
	fread(&tpw.neighbour_weight, sizeof(tpw.neighbour_weight), 1, fp_weight);
	fread(&tpw.response_match_weight, sizeof(tpw.response_match_weight), 1, fp_weight);
	fread(&tpw.self_atari_weight, sizeof(tpw.self_atari_weight), 1, fp_weight);
	fread(&tpw.last_move_distance_weight, sizeof(tpw.last_move_distance_weight), 1, fp_weight);
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		ResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		tree_response_weight_sorted.insert({ weight, val });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		NonResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		tree_nonresponse_weight_sorted.insert({ weight, val });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		Diamond12PatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		tree_diamond12_weight_sorted.insert({ weight, val });
	}
	fclose(fp_weight);

	// 表示
	printf("tree response pattern weight output num = %d\n", tree_response_weight_sorted.size());
	printf("tree nonresponse pattern weight output num = %d\n", tree_nonresponse_weight_sorted.size());
	printf("tree diamond12 pattern weight output num = %d\n", tree_diamond12_weight_sorted.size());

	printf("tree save atari weight = %f\n", tpw.save_atari_weight);
	printf("tree neighbour_weight = %f\n", tpw.neighbour_weight);
	printf("tree response match weight = %f\n", tpw.response_match_weight);
	printf("tree policy self atari weight = %f\n", tpw.self_atari_weight);
	for (int move = 0; move < 2; move++)
	{
		printf("tree policy last move distance %d = ", move);
		for (int i = 0; i < sizeof(tpw.last_move_distance_weight[0]) / sizeof(tpw.last_move_distance_weight[0][0]); i++)
		{
			printf("%f ", tpw.last_move_distance_weight[move][i]);
		}
		printf("\n");
	}

	// Top10
	printf("Top10\n");
	n = 0;
	for (auto itr = tree_response_weight_sorted.rbegin(); itr != tree_response_weight_sorted.rend() && n < 10; itr++, n++)
	{
		if (n == 0)
		{
			print_response_pattern(itr->second);
			printf("rotate\n");
			print_response_pattern(itr->second.rotate());
			printf("vmirror\n");
			print_response_pattern(itr->second.vmirror());
			printf("hmirror\n");
			print_response_pattern(itr->second.hmirror());
		}
		printf("tree response pattern weight : %llx : %f\n", itr->second, itr->first);
	}
	n = 0;
	for (auto itr = tree_nonresponse_weight_sorted.rbegin(); itr != tree_nonresponse_weight_sorted.rend() && n < 10; itr++, n++)
	{
		if (n == 0)
		{
			print_nonresponse_pattern(itr->second);
			printf("rotate\n");
			print_nonresponse_pattern(itr->second.rotate());
			printf("vmirror\n");
			print_nonresponse_pattern(itr->second.vmirror());
			printf("hmirror\n");
			print_nonresponse_pattern(itr->second.hmirror());
		}
		printf("tree nonresponse pattern weight : %lx : %f\n", itr->second, itr->first);
	}
	n = 0;
	for (auto itr = tree_diamond12_weight_sorted.rbegin(); itr != tree_diamond12_weight_sorted.rend() && n < 10; itr++, n++)
	{
		if (n == 0)
		{
			print_diamond12_pattern(itr->second);
			printf("rotate\n");
			print_diamond12_pattern(itr->second.rotate());
			printf("vmirror\n");
			print_diamond12_pattern(itr->second.vmirror());
			printf("hmirror\n");
			print_diamond12_pattern(itr->second.hmirror());
		}
		printf("tree diamond12 pattern weight : %llx : %f\n", itr->second, itr->first);
	}

	// Bottom10
	printf("Bottom10\n");
	n = 0;
	for (auto itr = tree_response_weight_sorted.begin(); itr != tree_response_weight_sorted.end() && n < 10; itr++, n++)
	{
		printf("tree response pattern weight : %llx : %f\n", itr->second, itr->first);
	}
	n = 0;
	for (auto itr = tree_nonresponse_weight_sorted.begin(); itr != tree_nonresponse_weight_sorted.end() && n < 10; itr++, n++)
	{
		printf("tree nonresponse pattern weight : %lx : %f\n", itr->second, itr->first);
	}
	n = 0;
	for (auto itr = tree_diamond12_weight_sorted.begin(); itr != tree_diamond12_weight_sorted.end() && n < 10; itr++, n++)
	{
		printf("tree diamond12 pattern weight : %llx : %f\n", itr->second, itr->first);
	}
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
	else if (wcscmp(argv[1], L"dump") == 0)
	{
		dump_weight();
	}

	return 0;
}