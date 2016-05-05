#include <math.h>
#include "../Board.h"
#include "Pattern.h"

inline int get_liberty_val(const int liberty_num)
{
	return (liberty_num >= 3) ? 3 : liberty_num;
}

// レスポンスパターン値取得(回転、対称形の最小値)
ResponsePatternVal get_response_pattern_key_min(const ResponsePatternVal& val)
{
	ResponsePatternVal min = val;

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

	return min;
}

// ノンレスポンスパターン値取得(回転、対称形の最小値)
NonResponsePatternVal get_nonresponse_pattern_key_min(const NonResponsePatternVal& val)
{
	NonResponsePatternVal min = val;

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

	return min;
}

ResponsePatternVal response_pattern(const Board& board, const XY xy, Color color)
{
	if (board.pre_xy == PASS)
	{
		return 0;
	}

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
	return get_response_pattern_key_min(val);
}

NonResponsePatternVal nonresponse_pattern(const Board& board, const XY xy, Color color)
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

	return get_nonresponse_pattern_key_min(val);
}
