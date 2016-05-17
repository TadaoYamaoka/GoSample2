#include <Windows.h>
#include <string>
#include <cassert>
#include <vector>
#include <chrono>
#include <algorithm>

#include "learn.h"
#include "../Board.h"
#include "../Random.h"
#include "Sgf.h"
#include "Hash.h"

using namespace std;

const int GRAPH_WIDTH = 800;
const int GRAPH_HEIGHT = 600;

float rollout_loss_data[GRAPH_WIDTH * 2];
float tree_loss_data[GRAPH_WIDTH * 2];
int loss_data_pos = 0;

HWND hMainWnd;
HPEN hPenRollout[2];
HPEN hPenTree[2];

void display_loss_graph();
void update_loss_graph();
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


// 学習係数
float eta = 0.01;

// 正則化係数
float ramda = 0.00000001;

struct WeightLoss
{
	float weight;
	float g2;
	int last_update_position;
};

// rollout policyの重み
WeightLoss rpw_save_atari_weight;
WeightLoss rpw_neighbour_weight;
//WeightLoss rpw_response_match_weight;
map<ResponsePatternVal, WeightLoss> rpw_response_pattern_weight;
map<NonResponsePatternVal, WeightLoss> rpw_nonresponse_pattern_weight;

// tree policyの重み
WeightLoss tpw_save_atari_weight;
WeightLoss tpw_neighbour_weight;
//WeightLoss tpw_response_match_weight;
map<ResponsePatternVal, WeightLoss> tpw_response_pattern_weight;
map<NonResponsePatternVal, WeightLoss> tpw_nonresponse_pattern_weight;
WeightLoss tpw_self_atari_weight;
WeightLoss tpw_last_move_distance_weight[2][17];
map<Diamond12PatternVal, WeightLoss> tpw_diamond12_pattern_weight;

// ハッシュキー衝突検出用
ResponsePatternVal response_pattern_collision[HASH_KEY_MAX];
NonResponsePatternVal nonresponse_pattern_collision[HASH_KEY_MAX];
Diamond12PatternVal diamond12_pattern_collision[HASH_KEY_MAX];

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

// 教師データに一致する手の勾配
inline float grad_weight_y(float y)
{
	return (y - 1.0f);
}

// 教師データに一致しない手の勾配
inline float grad_weight_y_etc(float y_etc)
{
	return y_etc;
}

// 教師データに一致する手の更新
inline void update_weight_y(WeightLoss& weight_loss, const float y, map<WeightLoss*, float>& update_weight_map)
{
	update_weight_map[&weight_loss] += grad_weight_y(y);
}

template <typename T, typename K>
void update_weight_map_y(T& weightmap, const K& key, const float y, map<WeightLoss*, float>& update_weight_map)
{
	auto itr = weightmap.find(key);
	if (itr == weightmap.end())
	{
		return;
	}

	update_weight_y(itr->second, y, update_weight_map);
}

// 教師データに一致しない手の更新
inline void update_weight_y_etc(WeightLoss& weight_loss, const float y_etc, map<WeightLoss*, float>& update_weight_map)
{
	update_weight_map[&weight_loss] += grad_weight_y_etc(y_etc);
}

template <typename T, typename K>
void update_weight_map_y_etc(T& weightmap, const K& key, const float y_etc, map<WeightLoss*, float>& update_weight_map)
{
	auto itr = weightmap.find(key);
	if (itr == weightmap.end())
	{
		return;
	}

	update_weight_y_etc(itr->second, y_etc, update_weight_map);
}

// 未反映のペナルティを反映
void penalty_pooled(WeightLoss& weight_loss, const int learned_position_num)
{
	if (ramda > 0)
	{
		int n = learned_position_num - weight_loss.last_update_position;

		if (n > 0)
		{
			if (weight_loss.weight - ramda * n > 0)
			{
				weight_loss.weight -= ramda * n;
			}
			else if (weight_loss.weight > 0)
			{
				weight_loss.weight = 0;
			}
			else if (weight_loss.weight + ramda * n < 0)
			{
				weight_loss.weight += ramda * n;
			}
			else if (weight_loss.weight < 0)
			{
				weight_loss.weight = 0;
			}
			weight_loss.last_update_position = learned_position_num;
		}
	}
}

// ペナルティの累積を反映して重みの値取得
inline float get_weight_val(WeightLoss& weight_loss, const int learned_position_num)
{
	// ペナルティの累積反映
	penalty_pooled(weight_loss, learned_position_num);

	return weight_loss.weight;
}

template <typename T, typename K>
float get_weight_map_val(T& weightmap, const K& key, const int learned_position_num)
{
	auto itr = weightmap.find(key);
	if (itr == weightmap.end())
	{
		return 0;
	}

	return get_weight_val(itr->second, learned_position_num);
}

bool is_delete(FILE* fp, const wchar_t* infile)
{
	char buf[10000];
	// 1行目読み飛ばし
	fgets(buf, sizeof(buf), fp);
	// 2行目
	fgets(buf, sizeof(buf), fp);

	int len = strlen(buf);
	if (buf[len - 2] == ')')
	{
		return true;
	}

	if (strsearch(buf, len, ")(", 2) != nullptr)
	{
		return true;
	}

	// ;で区切る
	char* next = strtok(buf, ";");
	int next_len = strlen(next);

	// 指導碁除外
	if (is_exclude(next))
	{
		return true;
	}

	// 置き碁除外
	if (strsearch(next, next_len, "AB[", 3) != nullptr)
	{
		return true;
	}

	// 19路以外除外
	if (strsearch(next, next_len, "SZ[19]", 6) == nullptr)
	{
		return true;
	}

	// 結果取得
	Color win = get_win_from_re(next, infile);
	if (win == 0)
	{
		return true;
	}

	// 最後まで打つ
	Board board(19);
	int turn = 0;
	while ((next = strtok(NULL, ";")) != NULL)
	{
		Color color = get_color_from_sgf(next);
		if (color == 0) {
			continue;
		}

		XY xy = get_xy_from_sgf(next);
		MoveResult result = board.move(xy, color, false);
		if (result != SUCCESS)
		{
			fprintf(stderr, "%S, turn = %d, %s, move result error.\n", infile, turn, next);
			return true;
		}

		turn++;
	}

	return false;
}

// 不要な棋譜を削除
void clean_kifu(const wchar_t* dirs)
{
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

			wstring infile = finddir + L"\\" + win32fd.cFileName;

			FILE* fp = _wfopen(infile.c_str(), L"r");
			bool isdelete = is_delete(fp, infile.c_str());
			fclose(fp);

			if (isdelete)
			{
				// ファイル削除
				DeleteFile(infile.c_str());
			}
		} while (FindNextFile(hFind, &win32fd));
	}
}

int learn_pattern_sgf(const wchar_t* infile, int &learned_position_num, FILE* errfile)
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

		if (turn >= 8 && xy != PASS)
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
			int key_num = 0;

			// 重みの更新量
			map<WeightLoss*, float> update_weight_map;

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
					ResponsePatternVal response_val = response_pattern_min(board, txy, color);

					// ノンレスポンスパターン
					NonResponsePatternVal nonresponse_val = nonresponse_pattern_min(board, txy, color);

					// Diamond12パターン
					Diamond12PatternVal diamond12_val = diamond12_pattern_min(board, txy, color);

					bool is_save_atari_tmp = false;
					bool is_self_atari_tmp = false;
					bool is_neighbour_tmp = false;

					// パラメータ更新準備
					// rollout policy
					// 重みの線形和
					float rollout_weight_sum = get_weight_map_val(rpw_nonresponse_pattern_weight, nonresponse_val, learned_position_num);
					if (response_val != 0)
					{
						//rollout_weight_sum += get_weight_val(rpw_response_match_weight, learned_position_num);
						rollout_weight_sum += get_weight_map_val(rpw_response_pattern_weight, response_val, learned_position_num);
					}
					// アタリを助ける手か
					if (atari_save.bit_test(txy))
					{
						rollout_weight_sum += get_weight_val(rpw_save_atari_weight, learned_position_num);
					}
					// 直前の手に隣接する手か
					if (is_neighbour(board, txy))
					{
						rollout_weight_sum += get_weight_val(rpw_neighbour_weight, learned_position_num);
					}

					// 各手のsoftmaxを計算
					float rollout_e = expf(rollout_weight_sum);
					rollout_e_sum += rollout_e;
					//printf("turn = %d, rollout_weight_sum = %f, rollout_e = %f, rollout_e_sum = %f\n", turn, rollout_weight_sum, rollout_e, rollout_e_sum);


					// tree policy
					// 重みの線形和
					float tree_weight_sum = get_weight_map_val(tpw_nonresponse_pattern_weight, nonresponse_val, learned_position_num);
					if (response_val != 0)
					{
						//tree_weight_sum += get_weight_val(tpw_response_match_weight, learned_position_num);
						tree_weight_sum += get_weight_map_val(tpw_response_pattern_weight, response_val, learned_position_num);
					}
					// アタリを助ける手か
					if (atari_save.bit_test(txy))
					{
						tree_weight_sum += get_weight_val(tpw_save_atari_weight, learned_position_num);
						is_save_atari_tmp = true;
					}
					// 直前の手に隣接する手か
					if (is_neighbour(board, txy))
					{
						tree_weight_sum += get_weight_val(tpw_neighbour_weight, learned_position_num);
						is_neighbour_tmp = true;
					}
					// アタリになる手か
					if (board.is_self_atari(color, txy))
					{
						is_self_atari_tmp = true;
						tree_weight_sum += get_weight_val(tpw_self_atari_weight, learned_position_num);
					}
					// 直前2手からの距離
					for (int move = 0; move < 2; move++)
					{
						if (board.pre_xy[move] != PASS)
						{
							XY distance = get_distance(txy, board.pre_xy[move]);
							if (distance >= sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]))
							{
								distance = sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]) - 1;
							}
							tree_weight_sum += get_weight_val(tpw_last_move_distance_weight[move][distance], learned_position_num);
						}
					}
					// 12-point diamondパターン
					tree_weight_sum += get_weight_map_val(tpw_diamond12_pattern_weight, diamond12_val, learned_position_num);

					// 各手のsoftmaxを計算
					float tree_e = expf(tree_weight_sum);
					tree_e_sum += tree_e;
					//printf("turn = %d, tree_weight_sum = %f, tree_e = %f, tree_e_sum = %f\n", turn, tree_weight_sum, tree_e, tree_e_sum);


					// 教師データと一致する場合
					if (txy == xy)
					{
						rollout_e_y = rollout_e;
						tree_e_y = tree_e;
						key_y.xy = txy;
						key_y.response_val = response_val;
						key_y.nonresponse_val = nonresponse_val;
						key_y.diamond12_val = diamond12_val;
						key_y.is_save_atari = is_save_atari_tmp;
						key_y.is_self_atari = is_self_atari_tmp;
						key_y.is_neighbour = is_neighbour_tmp;
					}
					else {
						rollout_e_etc[key_num] = rollout_e;
						tree_e_etc[key_num] = tree_e;
						keys[key_num].xy = txy;
						keys[key_num].response_val = response_val;
						keys[key_num].nonresponse_val = nonresponse_val;
						keys[key_num].diamond12_val = diamond12_val;
						keys[key_num].is_save_atari = is_save_atari_tmp;
						keys[key_num].is_self_atari = is_self_atari_tmp;
						keys[key_num].is_neighbour = is_neighbour_tmp;
						key_num++;
					}
				}
			}

			if (rollout_e_y == 0 || tree_e_y == 0)
			{
				fprintf(errfile, "%S, turn = %d, %s, supervised data not match.\n", infile, turn, next);
				break;
			}

			// 教師データと一致する手のsoftmax
			float rollout_y = rollout_e_y / rollout_e_sum;
			//printf("rollout_y = %f\n", rollout_y);
			float tree_y = tree_e_y / tree_e_sum;

			// 教師データと一致する手のパラメータ更新
			if (key_y.nonresponse_val != 0) // 空白パターンは制約条件として更新しない
			{
				// rollout policy
				update_weight_map_y(rpw_nonresponse_pattern_weight, key_y.nonresponse_val, rollout_y, update_weight_map);
				// tree policy
				update_weight_map_y(tpw_nonresponse_pattern_weight, key_y.nonresponse_val, tree_y, update_weight_map);
			}
			if (key_y.response_val != 0)
			{
				// rollout policy
				//update_weight_y(rpw_response_match_weight, rollout_y, update_weight_map);
				update_weight_map_y(rpw_response_pattern_weight, key_y.response_val, rollout_y, update_weight_map);
				// tree policy
				//update_weight_y(tpw_response_match_weight, tree_y, update_weight_map);
				update_weight_map_y(tpw_response_pattern_weight, key_y.response_val, tree_y, update_weight_map);
			}
			// アタリを助ける手か
			if (key_y.is_save_atari)
			{
				// rollout policy
				update_weight_y(rpw_save_atari_weight, rollout_y, update_weight_map);
				// tree policy
				update_weight_y(tpw_save_atari_weight, tree_y, update_weight_map);
			}
			// 直前の手に隣接する手か
			if (key_y.is_neighbour)
			{
				// rollout policy
				update_weight_y(rpw_neighbour_weight, rollout_y, update_weight_map);
				// tree policy
				update_weight_y(tpw_neighbour_weight, tree_y, update_weight_map);
			}
			// アタリになる手か
			if (key_y.is_self_atari)
			{
				// tree policy
				update_weight_y(tpw_self_atari_weight, tree_y, update_weight_map);
			}
			// 直前の2手からの距離
			for (int move = 0; move < 2; move++)
			{
				if (board.pre_xy[move] != PASS)
				{
					XY distance = get_distance(xy, board.pre_xy[move]);
					if (distance == 0 && move == 0)
					{
						fprintf(errfile, "%S, turn = %d, %s, distance error.\n", infile, turn, next);
						break;
					}
					if (distance >= sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]))
					{
						distance = sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]) - 1;
					}
					update_weight_y(tpw_last_move_distance_weight[move][distance], tree_y, update_weight_map);
					
				}
			}
			// 12-point diamondパターン
			update_weight_map_y(tpw_diamond12_pattern_weight, key_y.diamond12_val, tree_y, update_weight_map);

			// 損失関数
			rollout_loss += -logf(rollout_y);
			rollout_loss_cnt++;
			tree_loss += -logf(tree_y);
			tree_loss_cnt++;
			//printf("turn = %d, rollout_loss = %f, tree_loss = %f\n", turn, rollout_loss, tree_loss);

			// 教師データと一致しない手のパラメータ更新
			for (int i = 0; i < key_num; i++)
			{
				float rollout_y_etc = rollout_e_etc[i] / rollout_e_sum;
				float tree_y_etc = tree_e_etc[i] / tree_e_sum;
				if (keys[i].nonresponse_val != 0) // 空白パターンは制約条件として更新しない
				{
					// rollout policy
					update_weight_map_y_etc(rpw_nonresponse_pattern_weight, keys[i].nonresponse_val, rollout_y_etc, update_weight_map);
					// tree policy
					update_weight_map_y_etc(tpw_nonresponse_pattern_weight, keys[i].nonresponse_val, tree_y_etc, update_weight_map);
				}
				if (keys[i].response_val != 0)
				{
					// rollout policy
					//update_weight_y_etc(rpw_response_match_weight, rollout_y_etc, update_weight_map);
					update_weight_map_y_etc(rpw_response_pattern_weight, keys[i].response_val, rollout_y_etc, update_weight_map);
					// tree policy
					//update_weight_y_etc(tpw_response_match_weight, tree_y_etc, update_weight_map);
					update_weight_map_y_etc(tpw_response_pattern_weight, keys[i].response_val, tree_y_etc, update_weight_map);
				}
				// アタリを助ける手か
				if (keys[i].is_save_atari)
				{
					// rollout policy
					update_weight_y_etc(rpw_save_atari_weight, rollout_y_etc, update_weight_map);
					// tree policy
					update_weight_y_etc(tpw_save_atari_weight, tree_y_etc, update_weight_map);
				}
				// 直前の手に隣接する手か
				if (keys[i].is_neighbour)
				{
					// rollout policy
					update_weight_y_etc(rpw_neighbour_weight, rollout_y_etc, update_weight_map);
					// tree policy
					update_weight_y_etc(tpw_neighbour_weight, tree_y_etc, update_weight_map);
				}
				// アタリになる手か
				if (keys[i].is_self_atari)
				{
					// tree policy
					update_weight_y_etc(tpw_self_atari_weight, tree_y_etc, update_weight_map);
				}
				// 直前の2手からの距離
				for (int move = 0; move < 2; move++)
				{
					if (board.pre_xy[move] != PASS)
					{
						XY distance = get_distance(keys[i].xy, board.pre_xy[move]);
						if (distance == 0 && move == 0)
						{
							fprintf(errfile, "%S, turn = %d, %s, distance error.\n", infile, turn, next);
							break;
						}
						if (distance >= sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]))
						{
							distance = sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]) - 1;
						}
						update_weight_y_etc(tpw_last_move_distance_weight[move][distance], tree_y_etc, update_weight_map);
					}
				}
				// 12-point diamondパターン
				update_weight_map_y_etc(tpw_diamond12_pattern_weight, keys[i].diamond12_val, tree_y_etc, update_weight_map);
			}

			// 重み更新
			for (const auto itr : update_weight_map)
			{
				WeightLoss* update_weight_loss = itr.first;
				float grad = itr.second;
				// AdaGrad
				update_weight_loss->g2 += grad * grad;
				update_weight_loss->weight -= eta * grad / sqrtf(update_weight_loss->g2);
			}
			//printf("nonresponse0 = %f, %f, %llx\n", get_weight_map_val(rpw_nonresponse_pattern_weight, 0, learned_position_num), rpw_nonresponse_pattern_weight[0].weight, &rpw_nonresponse_pattern_weight[0]);

			learned_position_num++;

			// 空白パターンにはペナルティをかけない
			rpw_nonresponse_pattern_weight[0].last_update_position = learned_position_num;
			tpw_nonresponse_pattern_weight[0].last_update_position = learned_position_num;
		}

		const MoveResult result = board.move(xy, color, false);
		if (result != SUCCESS)
		{
			fprintf(errfile, "%S, turn = %d, %s, move result error.\n", infile, turn, next);
			break;
		}

		turn++;
	}

	// 損失関数の平均値表示
	rollout_loss_data[loss_data_pos] = rollout_loss / rollout_loss_cnt;
	tree_loss_data[loss_data_pos] = tree_loss / tree_loss_cnt;
	printf("%S : rollout loss = %.5f, tree loss = %.5f\n", infile, rollout_loss_data[loss_data_pos], tree_loss_data[loss_data_pos]);
	loss_data_pos++;
	if (loss_data_pos >= sizeof(rollout_loss_data) / sizeof(rollout_loss_data[0]))
	{
		loss_data_pos = 0;
	}

	fclose(fp);

	return 1;
}

void read_pattern(RolloutPolicyWeight& rpw, TreePolicyWeight& tpw)
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

		rpw.response_pattern_weight.insert({ response, 0.0f });
		tpw.response_pattern_weight.insert({ response, 0.0f });
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

		rpw.nonresponse_pattern_weight.insert({ nonresponse, 0.0f });
		tpw.nonresponse_pattern_weight.insert({ nonresponse, 0.0f });
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

		tpw.diamond12_pattern_weight.insert({ diamond12, 0.0f });
	}
	fclose(fp);
}

void write_weight(const RolloutPolicyWeight& rpw, const TreePolicyWeight& tpw)
{
	// 重み出力
	// rollout policy
	FILE* fp_weight = fopen("rollout.bin", "wb");
	fwrite(&rpw.save_atari_weight, sizeof(rpw.save_atari_weight), 1, fp_weight);
	fwrite(&rpw.neighbour_weight, sizeof(rpw.neighbour_weight), 1, fp_weight);
	//fwrite(&rpw.response_match_weight, sizeof(rpw.response_match_weight), 1, fp_weight);
	int num = rpw.response_pattern_weight.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = rpw.response_pattern_weight.rbegin(); itr != rpw.response_pattern_weight.rend(); itr++)
	{
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
	}
	num = rpw.nonresponse_pattern_weight.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = rpw.nonresponse_pattern_weight.rbegin(); itr != rpw.nonresponse_pattern_weight.rend(); itr++)
	{
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
	}
	fclose(fp_weight);

	// tree policy
	fp_weight = fopen("tree.bin", "wb");
	fwrite(&tpw.save_atari_weight, sizeof(tpw.save_atari_weight), 1, fp_weight);
	fwrite(&tpw.neighbour_weight, sizeof(tpw.neighbour_weight), 1, fp_weight);
	//fwrite(&tpw.response_match_weight, sizeof(tpw.response_match_weight), 1, fp_weight);
	fwrite(&tpw.self_atari_weight, sizeof(tpw.self_atari_weight), 1, fp_weight);
	fwrite(&tpw.last_move_distance_weight, sizeof(tpw.last_move_distance_weight), 1, fp_weight);
	num = tpw.response_pattern_weight.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw.response_pattern_weight.rbegin(); itr != tpw.response_pattern_weight.rend(); itr++)
	{
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
	}
	num = tpw.nonresponse_pattern_weight.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw.nonresponse_pattern_weight.rbegin(); itr != tpw.nonresponse_pattern_weight.rend(); itr++)
	{
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
	}
	num = tpw.diamond12_pattern_weight.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw.diamond12_pattern_weight.rbegin(); itr != tpw.diamond12_pattern_weight.rend(); itr++)
	{
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
	}
	fclose(fp_weight);
}

void init_weight()
{
	RolloutPolicyWeight rpw;
	TreePolicyWeight tpw;

	// 重み初期化
	rpw.save_atari_weight = 0.0f;
	rpw.neighbour_weight = 0.0f;
	//rpw.response_match_weight = 0.0f;

	tpw.save_atari_weight = 0.0f;
	tpw.neighbour_weight = 0.0f;
	//tpw.response_match_weight = 0.0f;
	tpw.self_atari_weight = 0.0f;
	for (int move = 0; move < 2; move++)
	{
		for (int i = 0; i < sizeof(tpw.last_move_distance_weight[0]) / sizeof(tpw.last_move_distance_weight[0][0]); i++)
		{
			tpw.last_move_distance_weight[move][i] = 0.0f;
		}
	}

	// パターンを読み込んで初期化
	read_pattern(rpw, tpw);

	printf("rollout policy response pattern weight num = %d\n", rpw.response_pattern_weight.size());
	printf("rollout policy nonresponse pattern weight num = %d\n", rpw.nonresponse_pattern_weight.size());
	printf("tree policy response pattern weight num = %d\n", tpw.response_pattern_weight.size());
	printf("tree policy nonresponse pattern weight num = %d\n", tpw.nonresponse_pattern_weight.size());
	printf("tree policy diamond12 pattern weight num = %d\n", tpw.diamond12_pattern_weight.size());

	// 重み出力
	write_weight(rpw, tpw);
}

void learn_pattern(const wchar_t* dirs, const int game_num, const int iteration_num, const float eta, const float ramda)
{
	::eta = eta;
	::ramda = ramda;

	printf("game_num = %d, iteration_num = %d, eta = %f, ramda = %f\n", game_num, iteration_num, eta, ramda);

	int learned_game_num = 0; // 学習局数
	int learned_position_num = 0; // 学習局面数

	// 重み読み込み
	// rollout policy
	FILE* fp_weight = fopen("rollout.bin", "rb");
	fread(&rpw_save_atari_weight.weight, sizeof(rpw_save_atari_weight.weight), 1, fp_weight);
	fread(&rpw_neighbour_weight.weight, sizeof(rpw_neighbour_weight.weight), 1, fp_weight);
	//fread(&rpw_response_match_weight.weight, sizeof(rpw_response_match_weight.weight), 1, fp_weight);
	int num;
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		ResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		rpw_response_pattern_weight.insert({ val, { weight, 0 } });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		NonResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		rpw_nonresponse_pattern_weight.insert({ val, { weight, 0 } });
	}
	fclose(fp_weight);

	// tree policy
	fp_weight = fopen("tree.bin", "rb");
	fread(&tpw_save_atari_weight.weight, sizeof(tpw_save_atari_weight.weight), 1, fp_weight);
	fread(&tpw_neighbour_weight.weight, sizeof(tpw_neighbour_weight.weight), 1, fp_weight);
	//fread(&tpw_response_match_weight.weight, sizeof(tpw_response_match_weight.weight), 1, fp_weight);
	fread(&tpw_self_atari_weight.weight, sizeof(tpw_self_atari_weight.weight), 1, fp_weight);
	for (int move = 0; move < 2; move++)
	{
		for (int i = 0; i < sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]); i++)
		{
			fread(&tpw_last_move_distance_weight[move][i].weight, sizeof(tpw_last_move_distance_weight[move][i].weight), 1, fp_weight);
		}
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		ResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		tpw_response_pattern_weight.insert({ val, { weight, 0 } });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		NonResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		tpw_nonresponse_pattern_weight.insert({ val, { weight, 0 } });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		Diamond12PatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		tpw_diamond12_pattern_weight.insert({ val, { weight, 0 } });
	}
	fclose(fp_weight);

	// 入力ディレクトリ
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

	// ファイル一覧
	vector<wstring> filelist;
	vector<wstring> filelist_learn;
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

			// ファイル一覧に追加
			if (game_num == 0)
			{
				filelist_learn.push_back((finddir + L"\\" + win32fd.cFileName).c_str());
			}
			else
			{
				filelist.push_back((finddir + L"\\" + win32fd.cFileName).c_str());
			}
		} while (FindNextFile(hFind, &win32fd));
	}

	// ランダムで選択
	Random random;
	for (int i = 0; i < game_num; i++)
	{
		int select = random.random() % filelist.size();
		filelist_learn.push_back(filelist[select]);
	}

	// 損失のグラフ表示
	display_loss_graph();

	FILE* errfile = fopen("error.log", "w");

	// 時間計測
	auto start = chrono::system_clock::now();

	// 棋譜を読み込んで学習
	for (int i = 0; i < iteration_num; i++)
	{
		// シャッフル
		random_shuffle(filelist_learn.begin(), filelist_learn.end());

		for (wstring filepath : filelist_learn)
		{
			// パターン学習
			learned_game_num += learn_pattern_sgf(filepath.c_str(), learned_position_num, errfile);

			// グラフ表示更新
			update_loss_graph();
		}
	}

	fclose(errfile);

	// 未反映のペナルティを反映
	// rollout policy
	penalty_pooled(rpw_save_atari_weight, learned_position_num);
	penalty_pooled(rpw_neighbour_weight, learned_position_num);
	//penalty_pooled(rpw_response_match_weight, learned_position_num);
	for (auto itr : rpw_response_pattern_weight)
	{
		penalty_pooled(itr.second, learned_position_num);
	}
	for (auto itr : rpw_nonresponse_pattern_weight)
	{
		penalty_pooled(itr.second, learned_position_num);
	}
	// tree policy
	penalty_pooled(tpw_save_atari_weight, learned_position_num);
	penalty_pooled(tpw_neighbour_weight, learned_position_num);
	//penalty_pooled(tpw_response_match_weight, learned_position_num);
	penalty_pooled(tpw_self_atari_weight, learned_position_num);
	for (int move = 0; move < 2; move++)
	{
		for (int i = 0; i < sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]); i++)
		{
			penalty_pooled(tpw_last_move_distance_weight[move][i], learned_position_num);
		}
	}
	for (auto itr : tpw_response_pattern_weight)
	{
		penalty_pooled(itr.second, learned_position_num);
	}
	for (auto itr : tpw_nonresponse_pattern_weight)
	{
		penalty_pooled(itr.second, learned_position_num);
	}
	for (auto itr : tpw_diamond12_pattern_weight)
	{
		penalty_pooled(itr.second, learned_position_num);
	}

	// 時間計測
	auto end = chrono::system_clock::now();
	auto elapse = end - start;
	printf("elapsed time = %lld ms\n", chrono::duration_cast<std::chrono::milliseconds>(elapse).count());

	// 学習結果表示
	// rollout policy
	printf("rollout policy response pattern weight num = %d\n", rpw_response_pattern_weight.size());
	printf("rollout policy nonresponse pattern weight num = %d\n", rpw_nonresponse_pattern_weight.size());

	// 重み順にソート
	multimap<float, ResponsePatternVal> rpw_response_weight_sorted;
	multimap<float, NonResponsePatternVal> rpw_nonresponse_weight_sorted;
	for (auto itr : rpw_response_pattern_weight)
	{
		if (itr.second.weight != 0)
		{
			rpw_response_weight_sorted.insert({ itr.second.weight, itr.first });
		}
	}
	for (auto itr : rpw_nonresponse_pattern_weight)
	{
		if (itr.second.weight != 0)
		{
			rpw_nonresponse_weight_sorted.insert({ itr.second.weight, itr.first });
		}
	}

	printf("rollout policy response pattern weight output num = %d\n", rpw_response_weight_sorted.size());
	printf("rollout policy nonresponse pattern weight output num = %d\n", rpw_nonresponse_weight_sorted.size());

	printf("rollout policy save atari weight = %f\n", rpw_save_atari_weight.weight);
	printf("rollout policy neighbour weight = %f\n", rpw_neighbour_weight.weight);
	//printf("rollout policy response match weight = %f\n", rpw_response_match_weight.weight);
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
	printf("tree policy response pattern weight num = %d\n", tpw_response_pattern_weight.size());
	printf("tree policy nonresponse pattern weight num = %d\n", tpw_nonresponse_pattern_weight.size());
	printf("tree policy diamond12 pattern weight num = %d\n", tpw_diamond12_pattern_weight.size());

	multimap<float, ResponsePatternVal> tpw_response_weight_sorted;
	multimap<float, NonResponsePatternVal> tpw_nonresponse_weight_sorted;
	multimap<float, Diamond12PatternVal> tpw_diamond12_weight_sorted;
	for (auto itr : tpw_response_pattern_weight)
	{
		if (itr.second.weight != 0)
		{
			tpw_response_weight_sorted.insert({ itr.second.weight, itr.first });
		}
	}
	for (auto itr : tpw_nonresponse_pattern_weight)
	{
		if (itr.second.weight != 0)
		{
			tpw_nonresponse_weight_sorted.insert({ itr.second.weight, itr.first });
		}
	}
	for (auto itr : tpw_diamond12_pattern_weight)
	{
		if (itr.second.weight != 0)
		{
			tpw_diamond12_weight_sorted.insert({ itr.second.weight, itr.first });
		}
	}

	printf("tree policy response pattern weight output num = %d\n", tpw_response_weight_sorted.size());
	printf("tree policy nonresponse pattern weight output num = %d\n", tpw_nonresponse_weight_sorted.size());
	printf("tree policy diamond12 pattern weight output num = %d\n", tpw_diamond12_weight_sorted.size());

	printf("tree policy save atari weight = %f\n", tpw_save_atari_weight.weight);
	printf("tree policy neighbour weight = %f\n", tpw_neighbour_weight.weight);
	//printf("tree policy response match weight = %f\n", tpw_response_match_weight.weight);
	printf("tree policy self atari weight = %f\n", tpw_self_atari_weight.weight);
	for (int move = 0; move < 2; move++)
	{
		printf("tree policy last move distance %d = ", move);
		for (int i = 0; i < sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]); i++)
		{
			printf("%f ", tpw_last_move_distance_weight[move][i].weight);
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
	fp_weight = fopen("rollout.bin", "wb");
	fwrite(&rpw_save_atari_weight.weight, sizeof(rpw_save_atari_weight.weight), 1, fp_weight);
	fwrite(&rpw_neighbour_weight.weight, sizeof(rpw_neighbour_weight.weight), 1, fp_weight);
	//fwrite(&rpw_response_match_weight.weight, sizeof(rpw_response_match_weight.weight), 1, fp_weight);
	num = rpw_response_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = rpw_response_weight_sorted.begin(); itr != rpw_response_weight_sorted.end(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	num = rpw_nonresponse_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = rpw_nonresponse_weight_sorted.begin(); itr != rpw_nonresponse_weight_sorted.end(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	fclose(fp_weight);

	// tree policy
	fp_weight = fopen("tree.bin", "wb");
	fwrite(&tpw_save_atari_weight.weight, sizeof(tpw_save_atari_weight.weight), 1, fp_weight);
	fwrite(&tpw_neighbour_weight.weight, sizeof(tpw_neighbour_weight.weight), 1, fp_weight);
	//fwrite(&tpw_response_match_weight.weight, sizeof(tpw_response_match_weight.weight), 1, fp_weight);
	fwrite(&tpw_self_atari_weight.weight, sizeof(tpw_self_atari_weight.weight), 1, fp_weight);
	for (int move = 0; move < 2; move++)
	{
		for (int i = 0; i < sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]); i++)
		{
			fwrite(&tpw_last_move_distance_weight[move][i].weight, sizeof(tpw_last_move_distance_weight[move][i].weight), 1, fp_weight);
		}
	}
	num = tpw_response_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw_response_weight_sorted.begin(); itr != tpw_response_weight_sorted.end(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	num = tpw_nonresponse_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw_nonresponse_weight_sorted.begin(); itr != tpw_nonresponse_weight_sorted.end(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	num = tpw_diamond12_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw_diamond12_weight_sorted.begin(); itr != tpw_diamond12_weight_sorted.end(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	fclose(fp_weight);

	printf("Press any key.\n");
	getchar();
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

		if (turn >= 8 && xy != PASS)
		{
			// 候補手一覧
			for (XY txy = BOARD_WIDTH + 1; txy < BOARD_MAX - BOARD_WIDTH; txy++)
			{
				if (board.is_empty(txy) && board.is_legal(txy, color, false) == SUCCESS)
				{
					// 候補手パターン
					// レスポンスパターン
					ResponsePatternVal response_val = response_pattern_min(board, txy, color);
					if (response_val != 0)
					{
						response_pattern_map[response_val]++;
					}

					// ノンレスポンスパターン
					NonResponsePatternVal nonresponse_val = nonresponse_pattern_min(board, txy, color);
					nonresponse_pattern_map[nonresponse_val]++;

					// 12-point diamondパターン
					Diamond12PatternVal diamond12_val = diamond12_pattern_min(board, txy, color);
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

int insert_pattern_hash_collision(ResponsePatternVal* collision, const ResponsePatternVal& val)
{
	const HashKey key = get_hash_key_response_pattern(val);

	// 衝突検出
	if (collision[key] == 0)
	{
		collision[key] = val;
		return 0;
	}
	else if (collision[key] != val)
	{
		return 1;
	}
}

int insert_pattern_hash_collision(NonResponsePatternVal* collision, const NonResponsePatternVal& val)
{
	const HashKey key = get_hash_key_nonresponse_pattern(val);

	// 衝突検出
	if (collision[key] == 0)
	{
		collision[key] = val;
		return 0;
	}
	else if (collision[key] != val)
	{
		return 1;
	}
}

int insert_pattern_hash_collision(Diamond12PatternVal* collision, const Diamond12PatternVal& val)
{
	const HashKey key = get_hash_key_diamond12_pattern(val);

	// 衝突検出
	if (collision[key] == 0)
	{
		collision[key] = val;
		return 0;
	}
	else if (collision[key] != val)
	{
		return 1;
	}
}

template<typename T, typename V>
int insert_rotated_pattern_hash_collision(T* collision, const V& val)
{
	int collision_num = insert_pattern_hash_collision(collision, val);

	// 90度回転
	V rot = val.rotate();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// 180度回転
	rot = val.rotate();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// 270度回転
	rot = val.rotate();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// 上下反転
	rot = val.vmirror();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// 90度回転
	rot = rot.rotate();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// 左右反転
	rot = val.hmirror();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// 90度回転
	rot = rot.rotate();
	collision_num += insert_pattern_hash_collision(collision, rot);

	return collision_num;
}

void check_hash()
{
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

		// 衝突検出
		collision_num += insert_pattern_hash_collision(response_pattern_collision, response);
	}
	fclose(fp);
	printf("response pattern collision num = %d\n", collision_num);

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

		// 衝突検出
		collision_num += insert_pattern_hash_collision(nonresponse_pattern_collision, nonresponse);
	}
	fclose(fp);
	printf("nonresponse pattern collision num = %d\n", collision_num);

	collision_num = 0;
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

		// 衝突検出
		collision_num += insert_pattern_hash_collision(diamond12_pattern_collision, diamond12);
	}
	fclose(fp);
	printf("diamond12 pattern collision num = %d\n", collision_num);
}

inline const char* get_color_str(const Color color)
{
	return color == BLACK ? "●" : (color == WHITE ? "○" :  "　");
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
		printf("| %s%d", get_color_str(color), liberty_num);
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
	printf("| %s%d", get_color_str(color), liberty_num);
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
	printf("| %s%d", get_color_str(color), liberty_num);
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
	RolloutPolicyWeight rpw;
	TreePolicyWeight tpw;

	// 重み順にソート
	multimap<float, ResponsePatternVal> rollout_response_weight_sorted;
	multimap<float, NonResponsePatternVal> rollout_nonresponse_weight_sorted;
	multimap<float, ResponsePatternVal> tree_response_weight_sorted;
	multimap<float, NonResponsePatternVal> tree_nonresponse_weight_sorted;
	multimap<float, Diamond12PatternVal> tree_diamond12_weight_sorted;

	// 重み読み込み
	// rollout policy
	FILE* fp_weight = fopen("rollout.bin", "rb");
	if (fp_weight == NULL)
	{
		fprintf(stderr, "rollout.bin file open error.\n");
	}
	fread(&rpw.save_atari_weight, sizeof(rpw.save_atari_weight), 1, fp_weight);
	fread(&rpw.neighbour_weight, sizeof(rpw.neighbour_weight), 1, fp_weight);
	//fread(&rpw.response_match_weight, sizeof(rpw.response_match_weight), 1, fp_weight);
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
	//printf("rollout response match weight = %f\n", rpw.response_match_weight);

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
	if (fp_weight == NULL)
	{
		fprintf(stderr, "tree.bin file open error.\n");
	}
	fread(&tpw.save_atari_weight, sizeof(tpw.save_atari_weight), 1, fp_weight);
	fread(&tpw.neighbour_weight, sizeof(tpw.neighbour_weight), 1, fp_weight);
	//fread(&tpw.response_match_weight, sizeof(tpw.response_match_weight), 1, fp_weight);
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
	//printf("tree response match weight = %f\n", tpw.response_match_weight);
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

void sort_weight()
{
	RolloutPolicyWeight rpw;
	TreePolicyWeight tpw;

	// 重み順にソート
	multimap<float, ResponsePatternVal> rpw_response_weight_sorted;
	multimap<float, NonResponsePatternVal> rpw_nonresponse_weight_sorted;
	multimap<float, ResponsePatternVal> tpw_response_weight_sorted;
	multimap<float, NonResponsePatternVal> tpw_nonresponse_weight_sorted;
	multimap<float, Diamond12PatternVal> tpw_diamond12_weight_sorted;

	// 重み読み込み
	// rollout policy
	FILE* fp_weight = fopen("rollout.bin", "rb");
	if (fp_weight == NULL)
	{
		fprintf(stderr, "rollout.bin file open error.\n");
	}
	fread(&rpw.save_atari_weight, sizeof(rpw.save_atari_weight), 1, fp_weight);
	fread(&rpw.neighbour_weight, sizeof(rpw.neighbour_weight), 1, fp_weight);
	//fread(&rpw.response_match_weight, sizeof(rpw.response_match_weight), 1, fp_weight);
	int num;
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		ResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		rpw_response_weight_sorted.insert({ weight, val });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		NonResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		rpw_nonresponse_weight_sorted.insert({ weight, val });
	}
	fclose(fp_weight);

	// tree policy
	fp_weight = fopen("tree.bin", "rb");
	if (fp_weight == NULL)
	{
		fprintf(stderr, "tree.bin file open error.\n");
	}
	fread(&tpw.save_atari_weight, sizeof(tpw.save_atari_weight), 1, fp_weight);
	fread(&tpw.neighbour_weight, sizeof(tpw.neighbour_weight), 1, fp_weight);
	//fread(&tpw.response_match_weight, sizeof(tpw.response_match_weight), 1, fp_weight);
	fread(&tpw.self_atari_weight, sizeof(tpw.self_atari_weight), 1, fp_weight);
	fread(&tpw.last_move_distance_weight, sizeof(tpw.last_move_distance_weight), 1, fp_weight);
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		ResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		tpw_response_weight_sorted.insert({ weight, val });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		NonResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		tpw_nonresponse_weight_sorted.insert({ weight, val });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		Diamond12PatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		tpw_diamond12_weight_sorted.insert({ weight, val });
	}
	fclose(fp_weight);

	// 重み出力
	// rollout policy
	fp_weight = fopen("rollout.bin", "wb");
	fwrite(&rpw.save_atari_weight, sizeof(rpw.save_atari_weight), 1, fp_weight);
	fwrite(&rpw.neighbour_weight, sizeof(rpw.neighbour_weight), 1, fp_weight);
	//fwrite(&rpw.response_match_weight, sizeof(rpw.response_match_weight), 1, fp_weight);
	num = rpw_response_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = rpw_response_weight_sorted.begin(); itr != rpw_response_weight_sorted.end(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	num = rpw_nonresponse_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = rpw_nonresponse_weight_sorted.begin(); itr != rpw_nonresponse_weight_sorted.end(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	fclose(fp_weight);

	// tree policy
	fp_weight = fopen("tree.bin", "wb");
	fwrite(&tpw.save_atari_weight, sizeof(tpw.save_atari_weight), 1, fp_weight);
	fwrite(&tpw.neighbour_weight, sizeof(tpw.neighbour_weight), 1, fp_weight);
	//fwrite(&tpw.response_match_weight, sizeof(tpw.response_match_weight), 1, fp_weight);
	fwrite(&tpw.self_atari_weight, sizeof(tpw.self_atari_weight), 1, fp_weight);
	for (int move = 0; move < 2; move++)
	{
		for (int i = 0; i < sizeof(tpw.last_move_distance_weight[0]) / sizeof(tpw.last_move_distance_weight[0][0]); i++)
		{
			fwrite(&tpw.last_move_distance_weight[move][i], sizeof(tpw.last_move_distance_weight[move][i]), 1, fp_weight);
		}
	}
	num = tpw_response_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw_response_weight_sorted.begin(); itr != tpw_response_weight_sorted.end(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	num = tpw_nonresponse_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw_nonresponse_weight_sorted.begin(); itr != tpw_nonresponse_weight_sorted.end(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	num = tpw_diamond12_weight_sorted.size();
	fwrite(&num, sizeof(num), 1, fp_weight);
	for (auto itr = tpw_diamond12_weight_sorted.begin(); itr != tpw_diamond12_weight_sorted.end(); itr++)
	{
		fwrite(&itr->second, sizeof(itr->second), 1, fp_weight);
		fwrite(&itr->first, sizeof(itr->first), 1, fp_weight);
	}
	fclose(fp_weight);
}

void display_loss_graph()
{
	WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
	wcex.style = 0;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = GetModuleHandle(NULL);
	wcex.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
	wcex.lpszMenuName = NULL;
	wcex.hCursor = LoadCursor(NULL, IDI_APPLICATION);
	wcex.lpszClassName = L"GoSampleLearn";

	RegisterClassEx(&wcex);

	DWORD style = WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX;
	RECT rc = { 0, 0, GRAPH_WIDTH, GRAPH_HEIGHT };
	AdjustWindowRect(&rc, style, NULL);

	hMainWnd = CreateWindow(
		wcex.lpszClassName,
		L"GoSample - learn",
		WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top,
		NULL, NULL, wcex.hInstance, NULL);
	if (hMainWnd == NULL)
	{
		return;
	}

	hPenRollout[0] = (HPEN)CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
	hPenTree[0] = (HPEN)CreatePen(PS_SOLID, 1, RGB(0, 0, 255));
	hPenRollout[1] = (HPEN)CreatePen(PS_SOLID, 1, RGB(255, 170, 170));
	hPenTree[1] = (HPEN)CreatePen(PS_SOLID, 1, RGB(170, 170, 255));

	ShowWindow(hMainWnd, SW_SHOW);
	UpdateWindow(hMainWnd);
}

void update_loss_graph()
{
	InvalidateRect(hMainWnd, NULL, FALSE);

	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			return;
		}
		if (GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hDC = BeginPaint(hWnd, &ps);

		// 損失グラフ表示
		PatBlt(hDC, 0, 0, GRAPH_WIDTH, GRAPH_HEIGHT, WHITENESS);

		const float max = 6.0f;

		HGDIOBJ hOld = SelectObject(hDC, hPenRollout[0]);

		int n0 = loss_data_pos / GRAPH_WIDTH;
		for (int i = 0, n = n0; i < 2; i++, n = (n + 1) % 2)
		{
			int offset = GRAPH_WIDTH * n;
			// rollout policy
			SelectObject(hDC, hPenRollout[i]);
			MoveToEx(hDC, 0, GRAPH_HEIGHT - rollout_loss_data[offset] / max * GRAPH_HEIGHT, NULL);
			for (int x = 1; x < GRAPH_WIDTH; x++)
			{
				LineTo(hDC, x, GRAPH_HEIGHT - rollout_loss_data[offset + x] / max * GRAPH_HEIGHT);

				if (n == n0 && offset + x >= loss_data_pos)
				{
					break;
				}
			}

			// tree policy
			SelectObject(hDC, hPenTree[i]);
			MoveToEx(hDC, 0, GRAPH_HEIGHT - tree_loss_data[offset] / max * GRAPH_HEIGHT, NULL);
			for (int x = 1; x < GRAPH_WIDTH; x++)
			{
				LineTo(hDC, x, GRAPH_HEIGHT - tree_loss_data[offset + x] / max * GRAPH_HEIGHT);

				if (n == n0 && offset + x >= loss_data_pos)
				{
					break;
				}
			}
		}

		SelectObject(hDC, hOld);

		EndPaint(hWnd, &ps);

		return 0;
	}
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void print_pattern(const wchar_t* kind, const wchar_t* valstr)
{
	if (wcscmp(kind, L"response") == 0)
	{
		long long val = _wtoi64(valstr);
		print_response_pattern(val);
	}
	else if (wcscmp(kind, L"nonresponse") == 0)
	{
		int val = _wtoi(valstr);
		print_nonresponse_pattern(val);
	}
	else if (wcscmp(kind, L"diamond12") == 0)
	{
		long long val = _wtoi64(valstr);
		print_diamond12_pattern(val);
	}
}