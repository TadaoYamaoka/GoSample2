#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>
#include "../Board.h"
#include "Sgf.h"

using namespace std;

int make_cnn_features_sgf(const wchar_t* infile, FILE* ofp);

void make_cnn_features(const wchar_t* dirs)
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

	FILE* ofp = fopen("cnn_features.bin", "wb");
	FILE* ofp_list = fopen("cnn_features_list.txt", "w");

	// 棋譜を読み込んで特徴を抽出
	for (auto file : filelist)
	{
		// 特徴を抽出
		int learned_position_num = make_cnn_features_sgf(file.c_str(), ofp);

		// 一覧出力
		fprintf(ofp_list, "%S\t%d\n", file.c_str(), learned_position_num);
	}

	fclose(ofp);
	fclose(ofp_list);
}

int make_cnn_features_sgf(const wchar_t* infile, FILE* ofp)
{
	FILE* fp = _wfopen(infile, L"r");
	char buf[10000];
	// 1行目読み飛ばし
	fgets(buf, sizeof(buf), fp);
	// 2行目
	fgets(buf, sizeof(buf), fp);

	// ;で区切る
	char* next = strtok(buf, ";");

	Board board(19);

	int turn = 0;
	while ((next = strtok(NULL, ";")) != NULL)
	{
		Color color = get_color_from_sgf(next);
		if (color == 0) {
			continue;
		}

		XY xy = get_xy_from_sgf(next);

		BitBoard<19 * 19> player_color;
		BitBoard<19 * 19> opponent_color;

		player_color.set_all_zero();
		opponent_color.set_all_zero();

		for (XY y = 1; y <= BOARD_SIZE; y++)
		{
			for (XY x = 1; x <= BOARD_SIZE; x++)
			{
				XY xy = y * BOARD_WIDTH + x;
				Color color_xy = board[xy];

				XY idx = y * BOARD_SIZE + x;
				if (color_xy == color)
				{
					player_color.bit_test_and_set(idx);
				}
				else if (color_xy == opponent(color))
				{
					opponent_color.bit_test_and_set(idx);
				}
			}
		}

		// 出力
		XY x_out = get_x(xy) - 1;
		XY y_out = get_y(xy) - 1;
		XY xy_out = y_out * BOARD_SIZE + x_out;
		fwrite(&xy_out, sizeof(xy_out), 1, ofp);
		fwrite(&player_color, sizeof(player_color), 1, ofp);
		fwrite(&opponent_color, sizeof(opponent_color), 1, ofp);

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
