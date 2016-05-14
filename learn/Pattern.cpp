#include <math.h>
#include "../Board.h"
#include "Pattern.h"

// パターン値取得(回転、対称形の最小値)
template <typename T>
T get_min_pattern_key(const T& val)
{
	T min = val;

	// 90度回転
	T rot = val.rotate();
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

	return min;
}

Diamond12PatternVal diamond12_pattern_min(const Board& board, const XY xy, const Color color)
{
	Diamond12PatternVal val = get_diamon12_pattern_val(board, xy, color);
	if (val == 0)
	{
		return 0;
	}
	return get_min_pattern_key(val);
}

ResponsePatternVal response_pattern_min(const Board& board, const XY xy, const Color color)
{
	if (board.pre_xy[0] <= PASS)
	{
		return 0;
	}

	// 直前の手の12ポイント範囲内か
	XY d = xy - board.pre_xy[0];
	XY dx = get_x(d);
	XY dy = get_y(d);
	if (abs(dx) + abs(dy) > 2)
	{
		return 0;
	}

	ResponsePatternVal val = get_diamon12_pattern_val(board, board.pre_xy[0], color);

	val.vals.move_pos = (dy + 2) * 5 + (dx + 2);
	return get_min_pattern_key(val);
}

// 計算済みの12point diamondパターンを使ってレスポンスパターンを計算
ResponsePatternVal response_pattern(const Board& board, const XY xy, const Color color, const ResponsePatternVal& base)
{
	if (board.pre_xy[0] <= PASS)
	{
		return 0;
	}

	// 直前の手の12ポイント範囲内か
	XY d = xy - board.pre_xy[0];
	XY dx = get_x(d);
	XY dy = get_y(d);
	if (abs(dx) + abs(dy) > 2)
	{
		return 0;
	}

	ResponsePatternVal val = base;

	val.vals.move_pos = (dy + 2) * 5 + (dx + 2);
	return val;
}

NonResponsePatternVal nonresponse_pattern(const Board& board, const XY xy, const Color color)
{
	NonResponsePatternVal val = { 0 };

	// 黒を基準にする
	const unsigned int color_mask = (color == BLACK) ? 0b00 : 0b11;

	// 1段目
	XY xyp = xy - BOARD_WIDTH - 1;
	if (board.is_stone(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.pattern_val ^ color_mask);
	}
	xyp++;
	if (board.is_stone(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.pattern_val ^ color_mask) << (4 * 1);
	}
	xyp++;
	if (board.is_stone(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.pattern_val ^ color_mask) << (4 * 2);
	}

	// 2段目
	xyp = xy - 1;
	if (board.is_stone(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.pattern_val ^ color_mask) << (4 * 3);
	}
	xyp += 2;
	if (board.is_stone(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.pattern_val ^ color_mask) << (4 * 4);
	}

	// 3段目
	xyp = xy + BOARD_WIDTH - 1;
	if (board.is_stone(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.pattern_val ^ color_mask) << (4 * 5);
	}
	xyp++;
	if (board.is_stone(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.pattern_val ^ color_mask) << (4 * 6);
	}
	xyp++;
	if (board.is_stone(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.pattern_val ^ color_mask) << (4 * 7);
	}

	return val;
}

NonResponsePatternVal nonresponse_pattern_min(const Board& board, const XY xy, const Color color)
{
	return get_min_pattern_key(nonresponse_pattern(board, xy, color));
}