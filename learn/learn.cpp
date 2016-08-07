#include <Windows.h>
#include <string>
#include <cassert>
#include <vector>
#include <chrono>
#include <algorithm>
#include <memory>
#include <random>
#include <iterator>
#include <algorithm>

#include "learn.h"
#include "../Board.h"
#include "Sgf.h"
#include "Hash.h"
#include "RolloutDataSource.h"
#include "Optimizers.h"

using namespace std;

const int GRAPH_WIDTH = 800;
const int GRAPH_HEIGHT = 600;

float rollout_loss_data[GRAPH_WIDTH * 2];
float tree_loss_data[GRAPH_WIDTH * 2];
float rollout_acc_data[GRAPH_WIDTH * 2];
float tree_acc_data[GRAPH_WIDTH * 2];
int loss_data_pos = 0;

HWND hMainWnd;
HPEN hPenRollout[2];
HPEN hPenTree[2];
HPEN hPenRolloutAcc[2];
HPEN hPenTreeAcc[2];

void display_loss_graph();
void update_loss_graph();
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


// �������W��
float ramda = 0.00000001;

struct WeightLoss
{
	float weight;
	int last_update_position;
	//SGD optimizer;
	//AdaGrad optimizer;
	RMSprop optimizer;
};

// rollout policy�̏d��
WeightLoss rpw_save_atari_weight;
WeightLoss rpw_neighbour_weight;
//WeightLoss rpw_response_match_weight;
map<ResponsePatternVal, WeightLoss> rpw_response_pattern_weight;
map<NonResponsePatternVal, WeightLoss> rpw_nonresponse_pattern_weight;

// tree policy�̏d��
WeightLoss tpw_save_atari_weight;
WeightLoss tpw_neighbour_weight;
//WeightLoss tpw_response_match_weight;
map<ResponsePatternVal, WeightLoss> tpw_response_pattern_weight;
map<NonResponsePatternVal, WeightLoss> tpw_nonresponse_pattern_weight;
WeightLoss tpw_self_atari_weight;
WeightLoss tpw_last_move_distance_weight[2][17];
map<Diamond12PatternVal, WeightLoss> tpw_diamond12_pattern_weight;

// �n�b�V���L�[�Փˌ��o�p
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

// �����񌟍�(UTF-8)
const char* strsearch(const char* str, const int len, const char* search, const int searchlen)
{
	// BM�@�Ō���
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

// ��������(UTF-8)
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

	// �w��(UTF-8)
	const char sido[] = { 0xe6, 0x8c, 0x87, 0xe5, 0xb0, 0x8e };
	// �A�}(UTF-8)
	const char ama[] = { 0xe3, 0x82, 0xa2, 0xe3, 0x83, 0x9e };
	// �G�k(UTF-8)
	const char zatsu[] = { 0xe9, 0x9b, 0x91, 0xe8, 0xab, 0x87 };

	if (strsearch(ev, ev_len, sido, sizeof(sido)))
	{
		return true;
	}
	else if (strsearch(ev, ev_len, ama, sizeof(ama)))
	{
		return true;
	}
	else if (strsearch(ev, ev_len, zatsu, sizeof(zatsu)))
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

// ���t�f�[�^�Ɉ�v�����̌��z
inline float grad_weight_y(float y)
{
	return (y - 1.0f);
}

// ���t�f�[�^�Ɉ�v���Ȃ���̌��z
inline float grad_weight_y_etc(float y_etc)
{
	return y_etc;
}

// ���t�f�[�^�Ɉ�v�����̍X�V
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

// ���t�f�[�^�Ɉ�v���Ȃ���̍X�V
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

// �����f�̃y�i���e�B�𔽉f
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

// �y�i���e�B�̗ݐς𔽉f���ďd�݂̒l�擾
inline float get_weight_val(WeightLoss& weight_loss, const int learned_position_num)
{
	// �y�i���e�B�̗ݐϔ��f
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
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unique_ptr<char[]> buf(new char[size + 1]);
	// �ǂݍ���
	fread(buf.get(), size, 1, fp);
	buf[size] = '\0';

	// ;�ŋ�؂�
	char* next = strtok(buf.get(), ";");

	// 1�ڂ�ǂݔ�΂�
	next = strtok(NULL, ";");

	int next_len = strlen(next);

	// �u���鏜�O
	if (strsearch(next, next_len, "AB[", 3) != nullptr)
	{
		fprintf(stderr, "%S, AB exist.\n", infile);
		return true;
	}

	// 19�H�ȊO���O
	if (strsearch(next, next_len, "SZ[19]", 6) == nullptr)
	{
		fprintf(stderr, "%S, size is not 19.\n", infile);
		return true;
	}

	// ���ʎ擾
	Color win = get_win_from_re(next, infile);
	if (win == 0)
	{
		return true;
	}

	// �Ō�܂őł�
	Board board(19);
	int turn = 0;
	while ((next = strtok(NULL, ";")) != NULL)
	{
		Color color = get_color_from_sgf(next);
		if (color == 0) {
			if (turn == 0 || next[0] == ')')
			{
				continue;
			}
			else
			{
				return true;
			}
		}

		XY xy = get_xy_from_sgf(next);
		if (xy != PASS && board[xy] != EMPTY)
		{
			fprintf(stderr, "%S, turn = %d, %s, stone exist error.\n", infile, turn, next);
			return true;
		}
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

// �s�v�Ȋ������폜
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
		// ���̓t�@�C���ꗗ
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

			FILE* fp = _wfopen(infile.c_str(), L"rb");
			bool isdelete = is_delete(fp, infile.c_str());
			fclose(fp);

			if (isdelete)
			{
				// �t�@�C���폜
				DeleteFile(infile.c_str());
			}
		} while (FindNextFile(hFind, &win32fd));
	}
}

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

void forward_rollout_tree_policy_inner(const RolloutDataSource& datasrc, const int learned_position_num, Board& board,
	float& rollout_e_sum, float& rollout_e_y, float rollout_e_etc[19 * 19], float& tree_e_sum, float& tree_e_y, float tree_e_etc[19 * 19],
	Key& key_y, // ���t�f�[�^�̃L�[
	Key keys[19 * 19], // ���t�f�[�^�̈ȊO�̃L�[
	int& key_num)
{
	const XY xy = datasrc.xy;

	// �r�b�g�{�[�h��W�J
	for (XY y = 0; y < 19; y++)
	{
		for (XY x = 0; x < 19; x++)
		{
			Color c = EMPTY;
			if (datasrc.player_color.bit_test(y * 19 + x))
			{
				c = BLACK;
			}
			else if (datasrc.opponent_color.bit_test(y * 19 + x))
			{
				c = WHITE;
			}

			if (c != EMPTY)
			{
				const XY move_xy = get_xy(x + 1, y + 1);
				MoveResult result = board.move(move_xy, c, false);
				if (result != MoveResult::SUCCESS)
				{
					fprintf(stderr, "move error.\n");
				}
			}
		}

	}

	board.pre_xy[0] = datasrc.pre_xy[0];
	board.pre_xy[1] = datasrc.pre_xy[1];

	// �F�͍��Œ�
	const Color color = BLACK;

	// �A�^�������������擾
	BitBoard<BOARD_BYTE_MAX> atari_save;
	board.get_atari_save(color, atari_save);

	// ����ꗗ
	for (XY txy = BOARD_WIDTH + 1; txy < BOARD_MAX - BOARD_WIDTH; txy++)
	{
		if (board.is_empty(txy) && board.is_legal(txy, color, false) == SUCCESS)
		{
			// ����p�^�[��
			// ���X�|���X�p�^�[��
			ResponsePatternVal response_val = response_pattern_min(board, txy, color);

			// �m�����X�|���X�p�^�[��
			NonResponsePatternVal nonresponse_val = nonresponse_pattern_min(board, txy, color);

			// Diamond12�p�^�[��
			Diamond12PatternVal diamond12_val = diamond12_pattern_min(board, txy, color);

			bool is_save_atari_tmp = false;
			bool is_self_atari_tmp = false;
			bool is_neighbour_tmp = false;

			// �p�����[�^�X�V����
			// rollout policy
			// �d�݂̐��`�a
			float rollout_weight_sum = get_weight_map_val(rpw_nonresponse_pattern_weight, nonresponse_val, learned_position_num);
			if (response_val != 0)
			{
				//rollout_weight_sum += get_weight_val(rpw_response_match_weight, learned_position_num);
				rollout_weight_sum += get_weight_map_val(rpw_response_pattern_weight, response_val, learned_position_num);
			}
			// �A�^����������肩
			if (atari_save.bit_test(txy))
			{
				rollout_weight_sum += get_weight_val(rpw_save_atari_weight, learned_position_num);
			}
			// ���O�̎�ɗאڂ���肩
			if (is_neighbour(board, txy))
			{
				rollout_weight_sum += get_weight_val(rpw_neighbour_weight, learned_position_num);
			}

			// �e���softmax���v�Z
			float rollout_e = expf(rollout_weight_sum);
			rollout_e_sum += rollout_e;
			//printf("turn = %d, rollout_weight_sum = %f, rollout_e = %f, rollout_e_sum = %f\n", turn, rollout_weight_sum, rollout_e, rollout_e_sum);


			// tree policy
			// �d�݂̐��`�a
			float tree_weight_sum = get_weight_map_val(tpw_nonresponse_pattern_weight, nonresponse_val, learned_position_num);
			if (response_val != 0)
			{
				//tree_weight_sum += get_weight_val(tpw_response_match_weight, learned_position_num);
				tree_weight_sum += get_weight_map_val(tpw_response_pattern_weight, response_val, learned_position_num);
			}
			// �A�^����������肩
			if (atari_save.bit_test(txy))
			{
				tree_weight_sum += get_weight_val(tpw_save_atari_weight, learned_position_num);
				is_save_atari_tmp = true;
			}
			// ���O�̎�ɗאڂ���肩
			if (is_neighbour(board, txy))
			{
				tree_weight_sum += get_weight_val(tpw_neighbour_weight, learned_position_num);
				is_neighbour_tmp = true;
			}
			// �A�^���ɂȂ�肩
			if (board.is_self_atari(color, txy))
			{
				is_self_atari_tmp = true;
				tree_weight_sum += get_weight_val(tpw_self_atari_weight, learned_position_num);
			}
			// ���O2�肩��̋���
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
			// 12-point diamond�p�^�[��
			tree_weight_sum += get_weight_map_val(tpw_diamond12_pattern_weight, diamond12_val, learned_position_num);

			// �e���softmax���v�Z
			float tree_e = expf(tree_weight_sum);
			tree_e_sum += tree_e;
			//printf("turn = %d, tree_weight_sum = %f, tree_e = %f, tree_e_sum = %f\n", turn, tree_weight_sum, tree_e, tree_e_sum);


			// ���t�f�[�^�ƈ�v����ꍇ
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
}

void learn_pattern_position(const RolloutDataSource& datasrc, const int learned_position_num, map<WeightLoss*, float>& update_weight_map, float& rollout_loss, float& tree_loss)
{
	const XY xy = datasrc.xy;

	Board board(19);

	float rollout_e_sum = 0;
	float rollout_e_y = 0;
	float rollout_e_etc[19 * 19] = { 0 };

	float tree_e_sum = 0;
	float tree_e_y = 0;
	float tree_e_etc[19 * 19] = { 0 };

	Key key_y; // ���t�f�[�^�̃L�[
	Key keys[19 * 19]; // ���t�f�[�^�̈ȊO�̃L�[
	int key_num = 0;

	// ���`�d
	forward_rollout_tree_policy_inner(datasrc, learned_position_num, board,
		rollout_e_sum, rollout_e_y, rollout_e_etc, tree_e_sum, tree_e_y, tree_e_etc,
		key_y, keys, key_num);

	if (rollout_e_y == 0 || tree_e_y == 0)
	{
		fprintf(stderr, "supervised data not match.\n");
		return;
	}

	// ���t�f�[�^�ƈ�v������softmax
	float rollout_y = rollout_e_y / rollout_e_sum;
	//printf("rollout_y = %f\n", rollout_y);
	float tree_y = tree_e_y / tree_e_sum;

	// ���t�f�[�^�ƈ�v�����̃p�����[�^�X�V
	if (key_y.nonresponse_val != 0) // �󔒃p�^�[���͐�������Ƃ��čX�V���Ȃ�
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
	// �A�^����������肩
	if (key_y.is_save_atari)
	{
		// rollout policy
		update_weight_y(rpw_save_atari_weight, rollout_y, update_weight_map);
		// tree policy
		update_weight_y(tpw_save_atari_weight, tree_y, update_weight_map);
	}
	// ���O�̎�ɗאڂ���肩
	if (key_y.is_neighbour)
	{
		// rollout policy
		update_weight_y(rpw_neighbour_weight, rollout_y, update_weight_map);
		// tree policy
		update_weight_y(tpw_neighbour_weight, tree_y, update_weight_map);
	}
	// �A�^���ɂȂ�肩
	if (key_y.is_self_atari)
	{
		// tree policy
		update_weight_y(tpw_self_atari_weight, tree_y, update_weight_map);
	}
	// ���O��2�肩��̋���
	for (int move = 0; move < 2; move++)
	{
		if (board.pre_xy[move] != PASS)
		{
			XY distance = get_distance(xy, board.pre_xy[move]);
			if (distance == 0 && move == 0)
			{
				fprintf(stderr, "distance error.\n");
				break;
			}
			if (distance >= sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]))
			{
				distance = sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]) - 1;
			}
			update_weight_y(tpw_last_move_distance_weight[move][distance], tree_y, update_weight_map);
					
		}
	}
	// 12-point diamond�p�^�[��
	update_weight_map_y(tpw_diamond12_pattern_weight, key_y.diamond12_val, tree_y, update_weight_map);

	// ���t�f�[�^�ƈ�v���Ȃ���̃p�����[�^�X�V
	for (int i = 0; i < key_num; i++)
	{
		float rollout_y_etc = rollout_e_etc[i] / rollout_e_sum;
		float tree_y_etc = tree_e_etc[i] / tree_e_sum;
		if (keys[i].nonresponse_val != 0) // �󔒃p�^�[���͐�������Ƃ��čX�V���Ȃ�
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
		// �A�^����������肩
		if (keys[i].is_save_atari)
		{
			// rollout policy
			update_weight_y_etc(rpw_save_atari_weight, rollout_y_etc, update_weight_map);
			// tree policy
			update_weight_y_etc(tpw_save_atari_weight, tree_y_etc, update_weight_map);
		}
		// ���O�̎�ɗאڂ���肩
		if (keys[i].is_neighbour)
		{
			// rollout policy
			update_weight_y_etc(rpw_neighbour_weight, rollout_y_etc, update_weight_map);
			// tree policy
			update_weight_y_etc(tpw_neighbour_weight, tree_y_etc, update_weight_map);
		}
		// �A�^���ɂȂ�肩
		if (keys[i].is_self_atari)
		{
			// tree policy
			update_weight_y_etc(tpw_self_atari_weight, tree_y_etc, update_weight_map);
		}
		// ���O��2�肩��̋���
		for (int move = 0; move < 2; move++)
		{
			if (board.pre_xy[move] != PASS)
			{
				XY distance = get_distance(keys[i].xy, board.pre_xy[move]);
				if (distance == 0 && move == 0)
				{
					fprintf(stderr, "distance error.\n");
					break;
				}
				if (distance >= sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]))
				{
					distance = sizeof(tpw_last_move_distance_weight[0]) / sizeof(tpw_last_move_distance_weight[0][0]) - 1;
				}
				update_weight_y_etc(tpw_last_move_distance_weight[move][distance], tree_y_etc, update_weight_map);
			}
		}
		// 12-point diamond�p�^�[��
		update_weight_map_y_etc(tpw_diamond12_pattern_weight, keys[i].diamond12_val, tree_y_etc, update_weight_map);
	}

	// �����֐�
	rollout_loss += -logf(rollout_y);
	tree_loss += -logf(tree_y);
}

void accuracy_rollout_tree_policy(const RolloutDataSource& datasrc, const int learned_position_num, float& rollout_acc, float& tree_acc)
{
	Board board(19);

	float rollout_e_sum = 0;
	float rollout_e_y = 0;
	float rollout_e_etc[19 * 19] = { 0 };

	float tree_e_sum = 0;
	float tree_e_y = 0;
	float tree_e_etc[19 * 19] = { 0 };

	Key key_y; // ���t�f�[�^�̃L�[
	Key keys[19 * 19]; // ���t�f�[�^�̈ȊO�̃L�[
	int key_num = 0;

	// ���`�d
	forward_rollout_tree_policy_inner(datasrc, learned_position_num, board,
		rollout_e_sum, rollout_e_y, rollout_e_etc, tree_e_sum, tree_e_y, tree_e_etc,
		key_y, keys, key_num);

	if (rollout_e_y == 0 || tree_e_y == 0)
	{
		fprintf(stderr, "supervised data not match.\n");
		return;
	}

	// ���t�f�[�^�ƈ�v������softmax
	rollout_acc = rollout_e_y / rollout_e_sum;
	tree_acc = tree_e_y / tree_e_sum;
}

void read_pattern_and_init_weight(RolloutPolicyWeight& rpw, TreePolicyWeight& tpw)
{
	FILE* fp = fopen("response.ptn", "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "response.ptn read error\n");
		return;
	}
	while (true)
	{
		ResponsePatternVal response;
		size_t size = fread(&response, sizeof(response), 1, fp);
		if (size == 0)
		{
			break;
		}

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
	while (true)
	{
		NonResponsePatternVal nonresponse;
		size_t size = fread(&nonresponse, sizeof(nonresponse), 1, fp);
		if (size == 0)
		{
			break;
		}

		rpw.nonresponse_pattern_weight.insert({ nonresponse, 0.0f });
		tpw.nonresponse_pattern_weight.insert({ nonresponse, 0.0f });
	}
	fclose(fp);
	// �󔒃p�^�[���̏d�݂�1�ɂ���(�������)
	rpw.nonresponse_pattern_weight[0] = 1.0f;
	tpw.nonresponse_pattern_weight[0] = 1.0f;

	fp = fopen("diamond12.ptn", "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "diamond12.ptn read error\n");
		return;
	}
	while (true)
	{
		Diamond12PatternVal diamond12;
		size_t size = fread(&diamond12, sizeof(diamond12), 1, fp);
		if (size == 0)
		{
			break;
		}

		tpw.diamond12_pattern_weight.insert({ diamond12, 0.0f });
	}
	fclose(fp);
}

void write_weight(const RolloutPolicyWeight& rpw, const TreePolicyWeight& tpw)
{
	// �d�ݏo��
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

void read_weight()
{
	// �d�ݓǂݍ���
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
		rpw_response_pattern_weight.insert({ val,{ weight, 0 } });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		NonResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		rpw_nonresponse_pattern_weight.insert({ val,{ weight, 0 } });
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
		tpw_response_pattern_weight.insert({ val,{ weight, 0 } });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		NonResponsePatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		tpw_nonresponse_pattern_weight.insert({ val,{ weight, 0 } });
	}
	fread(&num, sizeof(num), 1, fp_weight);
	for (int i = 0; i < num; i++)
	{
		Diamond12PatternVal val;
		float weight;
		fread(&val, sizeof(val), 1, fp_weight);
		fread(&weight, sizeof(weight), 1, fp_weight);
		tpw_diamond12_pattern_weight.insert({ val,{ weight, 0 } });
	}
	fclose(fp_weight);
}

void init_weight()
{
	RolloutPolicyWeight rpw;
	TreePolicyWeight tpw;

	// �d�ݏ�����
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

	// �p�^�[����ǂݍ���ŏ�����
	read_pattern_and_init_weight(rpw, tpw);

	printf("rollout policy response pattern weight num = %d\n", rpw.response_pattern_weight.size());
	printf("rollout policy nonresponse pattern weight num = %d\n", rpw.nonresponse_pattern_weight.size());
	printf("tree policy response pattern weight num = %d\n", tpw.response_pattern_weight.size());
	printf("tree policy nonresponse pattern weight num = %d\n", tpw.nonresponse_pattern_weight.size());
	printf("tree policy diamond12 pattern weight num = %d\n", tpw.diamond12_pattern_weight.size());

	// �d�ݏo��
	write_weight(rpw, tpw);
}

// position_num�͊w�K����ǖʐ�
// ���̓f�[�^�\�[�X�̐����傫���ꍇ�͌J��Ԃ��g�p����
void learn_pattern(const wchar_t* file, const int batch_size, int position_num)
{
	// �d�ݓǂݍ���
	// ����͎��O��init�����s���Ă��邱��
	read_weight();

	// ���̓f�[�^�\�[�X
	FILE *fp = _wfopen(file, L"rb");
	vector<RolloutDataSource> datasource;
	while (true)
	{
		datasource.push_back({});
		RolloutDataSource& data = datasource.back();
		int size = fread(&data, sizeof(data), 1, fp);
		if (size == 0)
		{
			break;
		}
	}
	fclose(fp);

	// �����_���ŕ��ёւ�
	random_device seed_gen;
	mt19937 engine(seed_gen());
	shuffle(datasource.begin(), datasource.end(), engine);

	// 1�����e�X�g�f�[�^�Ƃ���
	const int test_num = datasource.size() / 10;
	// �c����w�K�f�[�^�Ƃ���
	const int learn_num = datasource.size() - test_num;

	vector<RolloutDataSource> learndata;
	move(datasource.begin(), datasource.begin() + learn_num, back_inserter(learndata));

	vector<RolloutDataSource> testdata;
	move(datasource.begin() + learn_num, datasource.end(), back_inserter(testdata));

	// position_num��0�̂Ƃ��S��
	if (position_num == 0)
	{
		position_num = learn_num;
	}

	int iteration_num = position_num / batch_size;

	printf("batch_size = %d, position_num = %d, iteration_num = %d, learn_num = %d, test_num = %d\n", batch_size, position_num, iteration_num, learndata.size(), testdata.size());

	// �����̃O���t�\��
	display_loss_graph();

	// ���Ԍv��
	auto start = chrono::system_clock::now();

	// ������ǂݍ���Ŋw�K
	int learned_position_num = 0;
	// loss
	float rollout_loss = 0.0f;
	float tree_loss = 0.0f;
	int update_loss_graph_cnt = 0;
	for (int j = 0, learn_i = 0, test_i = 0; j < iteration_num; j++)
	{
		if (j != 0 && j % (learn_num / batch_size) == 0)
		{
			// �J��Ԃ��g�p����ꍇ�̓����_���ŕ��ёւ���
			random_device seed_gen;
			mt19937 engine(seed_gen());
			shuffle(learndata.begin(), learndata.end(), engine);
		}

		// �d�݂̍X�V��
		map<WeightLoss*, float> update_weight_map;

		// �~�j�o�b�`
		for (int b = 0; b < batch_size; b++)
		{
			// �p�^�[���w�K
			learn_pattern_position(learndata[learn_i % learn_num], learned_position_num, update_weight_map, rollout_loss, tree_loss);
			learn_i++;
			update_loss_graph_cnt++;
		}

		// �d�ݍX�V
		for (const auto itr : update_weight_map)
		{
			WeightLoss* update_weight_loss = itr.first;
			float grad = itr.second;

			grad /= batch_size;

			// �X�V
			update_weight_loss->weight -= update_weight_loss->optimizer(grad);
		}

		learned_position_num += batch_size;

		// �󔒃p�^�[���ɂ̓y�i���e�B�������Ȃ�
		rpw_nonresponse_pattern_weight[0].last_update_position = learned_position_num;
		tpw_nonresponse_pattern_weight[0].last_update_position = learned_position_num;

		// �O���t�\���X�V
		if (update_loss_graph_cnt >= 1000)
		{
			// �����֐��̕��ϒl
			rollout_loss_data[loss_data_pos] = rollout_loss / update_loss_graph_cnt;
			tree_loss_data[loss_data_pos] = tree_loss / update_loss_graph_cnt;

			// accuracy�v�Z
			float rollout_acc_sum = 0;
			float tree_acc_sum = 0;
			const int test_batch_size = 100;
			for (int b = 0; b < test_batch_size; b++)
			{
				float rollout_acc;
				float tree_acc;
				accuracy_rollout_tree_policy(testdata[test_i % test_num], learned_position_num, rollout_acc, tree_acc);
				test_i++;
				rollout_acc_sum += rollout_acc;
				tree_acc_sum += tree_acc;
			}
			rollout_acc_data[loss_data_pos] = rollout_acc_sum / test_batch_size;
			tree_acc_data[loss_data_pos] = tree_acc_sum / test_batch_size;

			printf("learned position = %d, ", learn_i);
			printf("rollout loss = %.5f, tree loss = %.5f, ", rollout_loss_data[loss_data_pos], tree_loss_data[loss_data_pos]);
			printf("rollout accuracy = %.5f, tree accuracy = %.5f\n", rollout_acc_data[loss_data_pos], tree_acc_data[loss_data_pos]);

			loss_data_pos++;
			if (loss_data_pos >= sizeof(rollout_loss_data) / sizeof(rollout_loss_data[0]))
			{
				loss_data_pos = 0;
			}

			update_loss_graph();

			fflush(stdout);

			rollout_loss = 0.0f;
			tree_loss = 0.0f;
			update_loss_graph_cnt = 0;
		}
	}

	// �����f�̃y�i���e�B�𔽉f
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

	// ���Ԍv��
	auto end = chrono::system_clock::now();
	auto elapse = end - start;
	printf("elapsed time = %lld ms\n", chrono::duration_cast<std::chrono::milliseconds>(elapse).count());

	// �󔒃p�^�[�����X�V����Ă��Ȃ����Ƃ��`�F�b�N
	printf("rollout policy empty nonresponse pattern weight = %f\n", rpw_nonresponse_pattern_weight[0].weight);
	printf("tree policy empty nonresponse pattern weight = %f\n", tpw_nonresponse_pattern_weight[0].weight);

	// �w�K���ʕ\��
	// rollout policy
	printf("rollout policy response pattern weight num = %d\n", rpw_response_pattern_weight.size());
	printf("rollout policy nonresponse pattern weight num = %d\n", rpw_nonresponse_pattern_weight.size());

	// �d�ݏ��Ƀ\�[�g
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

	// �d�ݏo��
	// rollout policy
	FILE* fp_weight = fopen("rollout.bin", "wb");
	fwrite(&rpw_save_atari_weight.weight, sizeof(rpw_save_atari_weight.weight), 1, fp_weight);
	fwrite(&rpw_neighbour_weight.weight, sizeof(rpw_neighbour_weight.weight), 1, fp_weight);
	//fwrite(&rpw_response_match_weight.weight, sizeof(rpw_response_match_weight.weight), 1, fp_weight);
	int num = rpw_response_weight_sorted.size();
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
	fflush(stdout);
	getchar();
}

int prepare_pattern_sgf(const wchar_t* infile, map<ResponsePatternVal, int>& response_pattern_map, map<NonResponsePatternVal, int>& nonresponse_pattern_map, map<Diamond12PatternVal, int>& diamond12_pattern_map)
{
	//printf("%S\n", infile);
	FILE* fp = _wfopen(infile, L"rb");
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unique_ptr<char[]> buf(new char[size + 1]);
	// �ǂݍ���
	fread(buf.get(), sizeof(char), size, fp);
	buf[size] = '\0';
	fclose(fp);

	// ;�ŋ�؂�
	char* next = strtok(buf.get(), ";");

	// 1�ڂ�ǂݔ�΂�
	next = strtok(NULL, ";");

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
			// ����ꗗ
			for (XY txy = BOARD_WIDTH + 1; txy < BOARD_MAX - BOARD_WIDTH; txy++)
			{
				if (board.is_empty(txy) && board.is_legal(txy, color, false) == SUCCESS)
				{
					// ����p�^�[��
					// ���X�|���X�p�^�[��
					ResponsePatternVal response_val = response_pattern_min(board, txy, color);
					if (response_val != 0)
					{
						response_pattern_map[response_val]++;
					}

					// �m�����X�|���X�p�^�[��
					NonResponsePatternVal nonresponse_val = nonresponse_pattern_min(board, txy, color);
					nonresponse_pattern_map[nonresponse_val]++;

					// 12-point diamond�p�^�[��
					Diamond12PatternVal diamond12_val = diamond12_pattern_min(board, txy, color);
					diamond12_pattern_map[diamond12_val]++;
				}
			}
		}

		board.move(xy, color, true);
		turn++;
	}

	return 1;
}

// �p�^�[�����o�ƕp�x����
void prepare_pattern(const wchar_t* dirs)
{
	int learned_game_num = 0; // �w�K�ǐ�

	map<ResponsePatternVal, int> response_pattern_map;
	map<NonResponsePatternVal, int> nonresponse_pattern_map;
	map<Diamond12PatternVal, int> diamond12_pattern_map;

	FILE *fp_dirlist = _wfopen(dirs, L"r");
	wchar_t dir[1024];

	// ������ǂݍ���Ŋw�K
	while (fgetws(dir, sizeof(dir) / sizeof(dir[0]), fp_dirlist) != NULL)
	{
		printf("%S", dir);

		// ���̓t�@�C���ꗗ
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

			// �p�^�[�����o
			learned_game_num += prepare_pattern_sgf((finddir + L"\\" + win32fd.cFileName).c_str(), response_pattern_map, nonresponse_pattern_map, diamond12_pattern_map);
		} while (FindNextFile(hFind, &win32fd));
	}
	fclose(fp_dirlist);

	printf("read game num = %d\n", learned_game_num);
	printf("response pattern num = %d\n", response_pattern_map.size());
	printf("nonresponse pattern num = %d\n", nonresponse_pattern_map.size());
	printf("diamond12 pattern num = %d\n", diamond12_pattern_map.size());

	// �p�x���ɕ��בւ�
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

	// �p�x���ɏo��
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

	// Top10�\��
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

	// �Փˌ��o
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

	// �Փˌ��o
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

	// �Փˌ��o
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

	// 90�x��]
	V rot = val.rotate();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// 180�x��]
	rot = val.rotate();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// 270�x��]
	rot = val.rotate();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// �㉺���]
	rot = val.vmirror();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// 90�x��]
	rot = rot.rotate();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// ���E���]
	rot = val.hmirror();
	collision_num += insert_pattern_hash_collision(collision, rot);

	// 90�x��]
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
	while (true)
	{
		ResponsePatternVal response;
		size_t size = fread(&response, sizeof(response), 1, fp);
		if (size == 0)
		{
			break;
		}

		// �Փˌ��o
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
	while (true)
	{
		NonResponsePatternVal nonresponse;
		size_t size = fread(&nonresponse, sizeof(nonresponse), 1, fp);
		if (size == 0)
		{
			break;
		}

		// �Փˌ��o
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
	while (true)
	{
		Diamond12PatternVal diamond12;
		size_t size = fread(&diamond12, sizeof(diamond12), 1, fp);
		if (size == 0)
		{
			break;
		}

		// �Փˌ��o
		collision_num += insert_pattern_hash_collision(diamond12_pattern_collision, diamond12);
	}
	fclose(fp);
	printf("diamond12 pattern collision num = %d\n", collision_num);
}

inline const char* get_color_str(const Color color)
{
	return color == BLACK ? "��" : (color == WHITE ? "��" :  "�@");
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
		printf("| �~ ");
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
	// 1�i��
	printf("| �@ | �@ ");
	print_response_pattern_stone(val, 1);
	printf("| �@ | �@ |\n");
	// 2�i��
	printf("| �@ ");
	print_response_pattern_stone(val, 2);
	print_response_pattern_stone(val, 3);
	print_response_pattern_stone(val, 4);
	printf("| �@ |\n");
	// 3�i��
	print_response_pattern_stone(val, 5);
	print_response_pattern_stone(val, 6);
	printf("| �� ");
	print_response_pattern_stone(val, 7);
	print_response_pattern_stone(val, 8);
	printf("|\n");
	// 4�i��
	printf("| �@ ");
	print_response_pattern_stone(val, 9);
	print_response_pattern_stone(val, 10);
	print_response_pattern_stone(val, 11);
	printf("| �@ |\n");
	// 5�i��
	printf("| �@ | �@ ");
	print_response_pattern_stone(val, 12);
	printf("| �@ | �@ |\n");
}

inline void print_nonresponse_pattern_stone(const NonResponsePatternVal& val, const int n)
{
	Color color = (val.val32 >> ((n - 1) * 4)) & 0b11ull;
	int liberty_num = (val.val32 >> ((n - 1) * 4 + 2)) & 0b11ull;
	printf("| %s%d", get_color_str(color), liberty_num);
}

void print_nonresponse_pattern(const NonResponsePatternVal& val)
{
	// 2�i��
	print_nonresponse_pattern_stone(val, 1);
	print_nonresponse_pattern_stone(val, 2);
	print_nonresponse_pattern_stone(val, 3);
	printf("|\n");
	// 3�i��
	print_nonresponse_pattern_stone(val, 4);
	printf("| �~ ");
	print_nonresponse_pattern_stone(val, 5);
	printf("|\n");
	// 4�i��
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
	// 1�i��
	printf("| �@ | �@ ");
	print_diamond12_pattern_stone(val, 1);
	printf("| �@ | �@ |\n");
	// 2�i��
	printf("| �@ ");
	print_diamond12_pattern_stone(val, 2);
	print_diamond12_pattern_stone(val, 3);
	print_diamond12_pattern_stone(val, 4);
	printf("| �@ |\n");
	// 3�i��
	print_diamond12_pattern_stone(val, 5);
	print_diamond12_pattern_stone(val, 6);
	printf("| �~ ");
	print_diamond12_pattern_stone(val, 7);
	print_diamond12_pattern_stone(val, 8);
	printf("|\n");
	// 4�i��
	printf("| �@ ");
	print_diamond12_pattern_stone(val, 9);
	print_diamond12_pattern_stone(val, 10);
	print_diamond12_pattern_stone(val, 11);
	printf("| �@ |\n");
	// 5�i��
	printf("| �@ | �@ ");
	print_diamond12_pattern_stone(val, 12);
	printf("| �@ | �@ |\n");
}

void dump_weight()
{
	RolloutPolicyWeight rpw;
	TreePolicyWeight tpw;

	// �d�ݏ��Ƀ\�[�g
	multimap<float, ResponsePatternVal> rollout_response_weight_sorted;
	multimap<float, NonResponsePatternVal> rollout_nonresponse_weight_sorted;
	multimap<float, ResponsePatternVal> tree_response_weight_sorted;
	multimap<float, NonResponsePatternVal> tree_nonresponse_weight_sorted;
	multimap<float, Diamond12PatternVal> tree_diamond12_weight_sorted;

	// �d�ݓǂݍ���
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

	// �\��
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

	// �\��
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

	// �d�ݏ��Ƀ\�[�g
	multimap<float, ResponsePatternVal> rpw_response_weight_sorted;
	multimap<float, NonResponsePatternVal> rpw_nonresponse_weight_sorted;
	multimap<float, ResponsePatternVal> tpw_response_weight_sorted;
	multimap<float, NonResponsePatternVal> tpw_nonresponse_weight_sorted;
	multimap<float, Diamond12PatternVal> tpw_diamond12_weight_sorted;

	// �d�ݓǂݍ���
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

	// �d�ݏo��
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
	// for accuracy
	hPenRolloutAcc[0] = (HPEN)CreatePen(PS_SOLID, 1, RGB(127, 0, 0));
	hPenTreeAcc[0] = (HPEN)CreatePen(PS_SOLID, 1, RGB(0, 0, 127));
	hPenRolloutAcc[1] = (HPEN)CreatePen(PS_SOLID, 1, RGB(127, 170, 170));
	hPenTreeAcc[1] = (HPEN)CreatePen(PS_SOLID, 1, RGB(170, 170, 127));

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

		// �����O���t�\��
		PatBlt(hDC, 0, 0, GRAPH_WIDTH, GRAPH_HEIGHT, WHITENESS);

		const float max = 6.0f;

		HGDIOBJ hOld = SelectObject(hDC, hPenRollout[0]);

		int n0 = loss_data_pos / GRAPH_WIDTH;
		for (int i = 0, n = n0; i < 2; i++, n = (n + 1) % 2)
		{
			int offset = GRAPH_WIDTH * n;
			// rollout policy loss
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

			// tree policy loss
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

			const float acc_max = 0.5f;
			// rollout policy accuracy
			SelectObject(hDC, hPenRolloutAcc[i]);
			MoveToEx(hDC, 0, GRAPH_HEIGHT - rollout_acc_data[offset] / acc_max * GRAPH_HEIGHT, NULL);
			for (int x = 1; x < GRAPH_WIDTH; x++)
			{
				LineTo(hDC, x, GRAPH_HEIGHT - rollout_acc_data[offset + x] / acc_max * GRAPH_HEIGHT);

				if (n == n0 && offset + x >= loss_data_pos)
				{
					break;
				}
			}

			// tree policy accuracy
			SelectObject(hDC, hPenTreeAcc[i]);
			MoveToEx(hDC, 0, GRAPH_HEIGHT - tree_acc_data[offset] / acc_max * GRAPH_HEIGHT, NULL);
			for (int x = 1; x < GRAPH_WIDTH; x++)
			{
				LineTo(hDC, x, GRAPH_HEIGHT - tree_acc_data[offset + x] / acc_max * GRAPH_HEIGHT);

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