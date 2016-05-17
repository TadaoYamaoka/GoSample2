#include <Windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "../BitBoard.h"
#include "../Board.h"
#include "Sgf.h"

using namespace std;

void nakade_search_sgf(const wchar_t* infile, int &learned_position_num);

void nakade_search(const wchar_t* dirs)
{
	int learned_game_num = 0; // 学習局数
	int learned_position_num = 0; // 学習局面数

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

	// 棋譜を読み込み
	for (wstring filepath : filelist)
	{
		// パターン学習
		nakade_search_sgf(filepath.c_str(), learned_position_num);
		learned_game_num++;
	}
}

void nakade_search_sgf(const wchar_t* infile, int &learned_position_num)
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
	XY nakade_xy = -1;
	int nakade_liberty_num;
	while ((next = strtok(NULL, ";")) != NULL)
	{
		turn++;

		const Color color = get_color_from_sgf(next);
		if (color == 0) {
			continue;
		}

		const XY xy = get_xy_from_sgf(next);

		if (nakade_xy == xy)
		{
			printf("%S, turn = %d, (x, y) = (%d, %d), liberty_num = %d, nakade\n", infile, turn, get_x(xy), get_y(xy), nakade_liberty_num);
		}

		const MoveResult result = board.move(xy, color, false);
		if (result != SUCCESS)
		{
			fprintf(stderr, "%S, turn = %d, %s, move result error.\n", infile, turn, next);
			break;
		}

		// 更新された連の周囲でナカデを探す
		for (int i = 0; i < board.pre_changed_group_num; i++)
		{
			const Group& group = board.groups[board.pre_changed_group[i]];
			// 呼吸点について
			for (int i = 0, hit_num = 0; hit_num < group.liberty_num; i++)
			{
				BitBoardPart bitboardpart = group.liberty_bitboard.get_bitboard_part(i);
				unsigned long idx;
				while (bit_scan_forward(&idx, bitboardpart))
				{
					bit_test_and_reset(&bitboardpart, idx);
					hit_num++;

					const XY xy0 = BIT * i + idx;

					int liberty_num = 1;

					// 上方向
					XY xy1 = xy0 - BOARD_WIDTH;
					if (board.is_empty(xy1))
					{
						liberty_num++;
						// 上
						if (!board.is_offboard(xy1 - BOARD_WIDTH) && board[xy1 - BOARD_WIDTH] != group.color)
						{
							continue;
						}
						// 左
						if (!board.is_offboard(xy1 - 1) && board[xy1 - 1] != group.color)
						{
							continue;
						}
						// 右
						if (!board.is_offboard(xy1 + 1) && board[xy1 + 1] != group.color)
						{
							continue;
						}
					}
					else if (!board.is_offboard(xy1) && board[xy1] != group.color)
					{
						continue;
					}
					// 左方向
					xy1 = xy0 - 1;
					if (board.is_empty(xy1))
					{
						liberty_num++;
						// 上
						if (!board.is_offboard(xy1 - BOARD_WIDTH) && board[xy1 - BOARD_WIDTH] != group.color)
						{
							continue;
						}
						// 左
						if (!board.is_offboard(xy1 - 1) && board[xy1 - 1] != group.color)
						{
							continue;
						}
						// 下
						if (!board.is_offboard(xy1 + BOARD_WIDTH) && board[xy1 + BOARD_WIDTH] != group.color)
						{
							continue;
						}
					}
					else if (!board.is_offboard(xy1) && board[xy1] != group.color)
					{
						continue;
					}
					// 右方向
					xy1 = xy0 + 1;
					if (board.is_empty(xy1))
					{
						liberty_num++;
						// 上
						if (!board.is_offboard(xy1 - BOARD_WIDTH) && board[xy1 - BOARD_WIDTH] != group.color)
						{
							continue;
						}
						// 右
						if (!board.is_offboard(xy1 + 1) && board[xy1 + 1] != group.color)
						{
							continue;
						}
						// 下
						if (!board.is_offboard(xy1 + BOARD_WIDTH) && board[xy1 + BOARD_WIDTH] != group.color)
						{
							continue;
						}
					}
					else if (!board.is_offboard(xy1) && board[xy1] != group.color)
					{
						continue;
					}
					// 下方向
					xy1 = xy0 + BOARD_WIDTH;
					if (board.is_empty(xy1))
					{
						liberty_num++;
						// 下
						if (!board.is_offboard(xy1 + BOARD_WIDTH) && board[xy1 + BOARD_WIDTH] != group.color)
						{
							continue;
						}
						// 左
						if (!board.is_offboard(xy1 - 1) && board[xy1 - 1] != group.color)
						{
							continue;
						}
						// 右
						if (!board.is_offboard(xy1 + 1) && board[xy1 + 1] != group.color)
						{
							continue;
						}
					}
					else if (!board.is_offboard(xy1) && board[xy1] != group.color)
					{
						continue;
					}

					if (liberty_num >= 3)
					{
						//printf("%S, turn = %d, (x, y) = (%d, %d), liberty_num = %d\n", infile, turn, get_x(xy0), get_y(xy0), liberty_num);
						nakade_xy = xy0;
						nakade_liberty_num = liberty_num;
					}
					else
					{
						nakade_xy = -1;
					}
				}
			}
		}
	}
	fclose(fp);
}
