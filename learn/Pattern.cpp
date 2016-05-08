#include <math.h>
#include "../Board.h"
#include "Pattern.h"

inline int get_liberty_val(const int liberty_num)
{
	return (liberty_num >= 3) ? 3 : liberty_num;
}

// �p�^�[���l�擾(��]�A�Ώ̌`�̍ŏ��l)
template <typename T>
T get_min_pattern_key(const T& val)
{
	T min = val;

	// 90�x��]
	T rot = val.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// 180�x��]
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// 270�x��]
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// �㉺���]
	rot = val.vmirror();
	if (rot < min)
	{
		min = rot;
	}

	// 90�x��]
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	// ���E���]
	rot = val.hmirror();
	if (rot < min)
	{
		min = rot;
	}

	// 90�x��]
	rot = rot.rotate();
	if (rot < min)
	{
		min = rot;
	}

	return min;
}

inline PatternVal64 get_diamon12_pattern_val(const Board& board, const XY xy, const Color color)
{
	PatternVal64 val64 = 0;

	// 1�i��
	XY xyp = xy - BOARD_WIDTH * 2;
	if (xyp > BOARD_WIDTH && !board.is_empty(xyp))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2));
	}

	// 2�i��
	xyp = xy - BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 1);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 2);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 3);
	}

	// 3�i��
	xyp = xy - 2;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp) && !board.is_offboard(xyp + 1))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 4);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 5);
	}
	xyp += 2;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 6);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp) && !board.is_offboard(xyp - 1))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 7);
	}

	// 4�i��
	xyp = xy + BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 8);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 9);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 10);
	}

	// 5�i��
	xyp = xy + BOARD_WIDTH * 2;
	if (xyp < BOARD_MAX - BOARD_WIDTH && !board.is_empty(xyp))
	{
		const Group& group = board.get_group(xyp);
		val64 |= (uint64_t)(group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 11);
	}

	// ������ɂ���
	if (color == WHITE)
	{
		// �F���]
		val64 ^= 0b001100110011001100110011001100110011001100110011ull;
	}

	return val64;
}

Diamond12PatternVal diamond12_pattern(const Board& board, const XY xy, const Color color)
{
	Diamond12PatternVal val = get_diamon12_pattern_val(board, xy, color);
	if (val == 0)
	{
		return 0;
	}
	return get_min_pattern_key(val);
}

ResponsePatternVal response_pattern(const Board& board, const XY xy, const Color color)
{
	if (board.pre_xy == PASS)
	{
		return 0;
	}

	// ���O�̎��12�|�C���g�͈͓���
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

NonResponsePatternVal nonresponse_pattern(const Board& board, const XY xy, const Color color)
{
	NonResponsePatternVal val = { 0 };

	// 1�i��
	XY xyp = xy - BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.color | (get_liberty_val(group.liberty_num) << 2));
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 1);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 2);
	}

	// 2�i��
	xyp = xy - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 3);
	}
	xyp += 2;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 4);
	}

	// 3�i��
	xyp = xy + BOARD_WIDTH - 1;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 5);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 6);
	}
	xyp++;
	if (!board.is_empty(xyp) && !board.is_offboard(xyp))
	{
		const Group& group = board.get_group(xyp);
		val.val32 |= (group.color | (get_liberty_val(group.liberty_num) << 2)) << (4 * 7);
	}

	// ������ɂ���
	if (color == WHITE)
	{
		// �F���]
		val.val32 ^= 0b00110011001100110011001100110011ul;
	}

	return get_min_pattern_key(val);
}
