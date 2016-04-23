#include <Windows.h>
#include "Human.h"

XY Human::select_move(Board& board, Color color)
{
	xy = -1;
	while (xy < 0)
	{
		Sleep(100);
		// メッセージ処理
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				return -1;
			}
			if (GetMessage(&msg, NULL, 0, 0))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
	XY ret = xy;
	xy = -1;
	return ret;
}

void Human::set_xy(XY xy)
{
	this->xy = xy;
}