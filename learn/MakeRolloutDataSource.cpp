#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>
#include "../Board.h"
#include "Sgf.h"
#include "RolloutDataSource.h"

using namespace std;

int make_rollout_datasource_sgf(const wchar_t* infile, FILE* ofp);

void make_rollout_datasource(const wchar_t* dirs)
{
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
			filelist.push_back((finddir + L"\\" + win32fd.cFileName).c_str());
		} while (FindNextFile(hFind, &win32fd));
	}

	FILE* ofp = fopen("rollout_datasource.bin", "wb");
	FILE* ofp_list = fopen("rollout_datasource_list.txt", "w");

	// 棋譜を読み込んで特徴を抽出
	for (auto file : filelist)
	{
		// 特徴を抽出
		int learned_position_num = make_rollout_datasource_sgf(file.c_str(), ofp);

		// 一覧出力
		fprintf(ofp_list, "%S\t%d\n", file.c_str(), learned_position_num);
	}

	fclose(ofp);
	fclose(ofp_list);
}

int make_rollout_datasource_sgf(const wchar_t* infile, FILE* ofp)
{
	FILE* fp = _wfopen(infile, L"r");
	char buf[10000];
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	// 読み込み
	fread(buf, size, 1, fp);
	buf[size] = '\0';

	// ;で区切る
	char* next = strtok(buf, ";");

	// 1つ目を読み飛ばす
	next = strtok(NULL, ";");

	Board board(19);

	int turn = 0;
	while ((next = strtok(NULL, ";")) != NULL)
	{
		Color color = get_color_from_sgf(next);
		if (color == 0) {
			continue;
		}

		RolloutDataSource data;
		XY xy = get_xy_from_sgf(next);
		if (xy != PASS)
		{
			data.xy = xy;
			data.pre_xy[0] = board.pre_xy[0];
			data.pre_xy[1] = board.pre_xy[1];

			data.player_color.set_all_zero();
			data.opponent_color.set_all_zero();

			for (XY y = 1; y <= BOARD_SIZE; y++)
			{
				for (XY x = 1; x <= BOARD_SIZE; x++)
				{
					XY xy = y * BOARD_WIDTH + x;
					Color color_xy = board[xy];

					XY idx = (y - 1) * BOARD_SIZE + (x - 1);
					if (color_xy == color)
					{
						data.player_color.bit_test_and_set(idx);
					}
					else if (color_xy == opponent(color))
					{
						data.opponent_color.bit_test_and_set(idx);
					}
				}
			}

			// 出力
			XY x_out = get_x(xy) - 1;
			XY y_out = get_y(xy) - 1;
			XY xy_out = y_out * BOARD_SIZE + x_out;
			fwrite(&data, sizeof(data), 1, ofp);
		}

		const MoveResult result = board.move(xy, color, false);
		if (result != SUCCESS)
		{
			fprintf(stderr, "%S, turn = %d, %s, move result error.\n", infile, turn, next);
			break;
		}

		turn++;
	}

	fclose(fp);

	return turn;
}
