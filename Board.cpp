#include "Board.h"
#include "learn/Pattern.h"

int BOARD_SIZE = 9;
int BOARD_WIDTH = BOARD_SIZE + 1;
int BOARD_STONE_MAX = BOARD_SIZE * BOARD_SIZE;
int BOARD_MAX = BOARD_WIDTH * (BOARD_SIZE + 2);
float KOMI = 6.5;
XY DIR4[4];

MoveResult Board::move(const XY xy, const Color color, const bool fill_eye_err)
{
	pre_changed_group_num = 0;
	pre_removed_group_num = 0;

	// �p�X�̏ꍇ
	if (xy == PASS) {
		ko = -1;
		push_pre_xy(PASS);
		return SUCCESS;
	}

	int tmp_ko;

	// ����4�����̘A
	int around_group_self[4];
	int around_group_self_num = 0;
	int around_group_capture[4];
	int around_group_capture_num = 0;
	int around_group_oponnent[4];
	int around_group_oponnent_num = 0;

	int capture_num = 0;

	// �ċz�_����v����A���擾
	for (GroupIndex idx = groups.begin(); idx != groups.end(); idx = groups.next(idx))
	{
		Group& group = groups[idx];
		if (group.hit_liberties(xy))
		{
			if (group.color == color)
			{
				around_group_self[around_group_self_num++] = idx;
			}
			else {
				// ��邱�Ƃ��ł��邩
				if (group.liberty_num == 1)
				{
					around_group_capture[around_group_capture_num++] = idx;
					capture_num += group.stone_num;
					if (group.stone_num == 1)
					{
						tmp_ko = group.stone[0];
					}
				}
				else {
					around_group_oponnent[around_group_oponnent_num++] = idx;
				}
			}
		}
	}

	int offboard_num = 0;
	int empty_num = 0;
	int alive_num = 0;
	// ����4�����̋�
	XY around_liberty[4] = { 0 };
	for (int i = 0; i < 4; i++)
	{
		XY xyd = xy + DIR4[i];

		if (board[xyd] == G_OFFBOARD)
		{
			offboard_num++;
			continue;
		}

		if (board[xyd] == G_NONE)
		{
			empty_num++;
			around_liberty[i] = 1;
			continue;
		}

		// �אڂ��鎩���̐F�̘A�̌ċz�_���Ȃ��Ȃ�Ȃ���
		if (groups[board[xyd]].color == color && groups[board[xyd]].liberty_num >= 2)
		{
			alive_num++;
		}
	}

	// ���E��
	if (capture_num == 0 && empty_num == 0 && alive_num == 0)
	{
		return ILLIGAL;
	}
	// �R�E
	if (xy == ko)
	{
		return KO;
	}
	// ��
	if (offboard_num + alive_num == 4 && fill_eye_err)
	{
		return EYE;
	}

	// �΂�u��
	if (around_group_self_num == 0)
	{
		// �A��ǉ�
		int idx = add_group(xy, color, around_liberty);

		// �A�ԍ��𖄂߂�
		board[xy] = idx;

		// �΂̐������Z
		stone_num[color]++;

		// �ύX�����A�ɒǉ�
		pre_changed_group[pre_changed_group_num++] = idx;

		// �󔒍폜
		empty_list.remove(xy);
	}
	else
	{
		// �A�ɐ΂�ǉ�
		Group& group0 = groups[around_group_self[0]];
		group0.add_stone_and_liberties(xy, around_liberty);

		// �A�ԍ��𖄂߂�
		board[xy] = around_group_self[0];

		// �΂̐������Z
		stone_num[color]++;

		// �ύX�����A�ɒǉ�
		pre_changed_group[pre_changed_group_num++] = around_group_self[0];

		// �󔒍폜
		empty_list.remove(xy);

		for (int i = 1; i < around_group_self_num; i++)
		{
			Group& groupi = groups[around_group_self[i]];

			// �A���Ȃ���
			group0.chain_group(xy, groupi);

			// �A�ԍ���u��������
			for (int j = 0; j < groupi.stone_num; j++)
			{
				board[groupi.stone[j]] = around_group_self[0];
			}

			// �A���폜
			remove_group(around_group_self[i]);

			// �ύX�����A�ɒǉ�
			//pre_changed_group[pre_changed_group_num++] = around_group_self[i]; // �Ȃ����A�Ɋ܂܂�邽�ߕs�v
		}
	}

	// �΂����
	for (int i = 0; i < around_group_capture_num; i++)
	{
		// �A�̂������ʒu���󔒂ɂ��Čċz�_��ǉ�����
		Group &remove = groups[around_group_capture[i]];
		for (int j = 0; j < remove.stone_num; j++)
		{
			// �A�̂������ʒu���󔒂ɂ���
			XY xyr = remove.stone[j];
			board[xyr] = G_NONE;

			// �󔒒ǉ�
			empty_list.add(xyr);

			// �אڂ���΂Ɍċz�_��ǉ�
			for (XY d : DIR4)
			{
				XY xyd = xyr + d;
				if (board[xyd] != G_OFFBOARD && board[xyd] != G_NONE)
				{
					if (groups[board[xyd]].color == color)
					{
						groups[board[xyd]].add_liberty(xyr);
					}
				}
			}
		}

		// ����̐΂̐������Z
		stone_num[opponent(color)] -= remove.stone_num;

		// �אڂ���G�̘A����A�ԍ����폜
		for (int j = 0, idx_offset = 0; j < remove.adjacent.get_part_size(); j++, idx_offset += BIT)
		{
			unsigned long idx_tmp;
			while (remove.adjacent.bit_scan_forward(j, &idx_tmp))
			{
				GroupIndex idx = idx_offset + idx_tmp;
				Group& adjacent_group = groups[idx];
				adjacent_group.adjacent.bit_test_and_reset(around_group_capture[i]);

				remove.adjacent.bit_reset_for_bsf(j); // �폜����A�Ȃ̂ōX�V���Ă悢

				// �ύX�����A�ɒǉ�
				pre_changed_group[pre_changed_group_num++] = idx;
			}
		}

		// �A���폜
		remove_group(around_group_capture[i]);

		// �ύX�����A�ɒǉ�
		pre_removed_group[pre_removed_group_num++] = around_group_capture[i];
	}

	// �אڂ���G�̘A�ɂ���
	for (int i = 0; i < around_group_oponnent_num; i++)
	{
		// �ċz�_�̍폜
		groups[around_group_oponnent[i]].remove_liberty(xy);

		// �ύX�����A�ɒǉ�
		if (groups[around_group_oponnent[i]].liberty_num < 3)
		{
			pre_changed_group[pre_changed_group_num++] = around_group_oponnent[i];
		}

		// �אڂ���G�̘A�ԍ���ǉ�
		groups[board[xy]].adjacent.bit_test_and_set(around_group_oponnent[i]);

		// �G�̘A�ɂ�������אڂ���A�Ƃ��Ēǉ�
		groups[around_group_oponnent[i]].adjacent.bit_test_and_set(board[xy]);
	}

	// �p�^�[���l�X�V
	for (int i = 0; i < pre_changed_group_num; i++)
	{
		Group& changed_group = groups[pre_changed_group[i]];
		changed_group.pattern_val = changed_group.color | get_pattern_liberty_val(changed_group.liberty_num);
	}

	// �R�E
	if (capture_num == 1 && groups[board[xy]].stone_num == 1 && groups[board[xy]].liberty_num == 1)
	{
		ko = tmp_ko;
	}
	else {
		ko = -1;
	}

	push_pre_xy(xy);
	return SUCCESS;
}

MoveResult Board::is_legal(const XY xy, const Color color, const bool fill_eye_err) const
{
	// ����4�����̘A
	int capture_num = 0;

	// �ċz�_����v����A���擾
	for (GroupIndex idx = groups.begin(); idx != groups.end(); idx = groups.next(idx))
	{
		const Group& group = groups[idx];
		if (group.hit_liberties(xy))
		{
			if (group.color == color)
			{
			}
			else {
				// ��邱�Ƃ��ł��邩
				if (group.liberty_num == 1)
				{
					capture_num += group.stone_num;
				}
			}
		}
	}

	int offboard_num = 0;
	int empty_num = 0;
	int alive_num = 0;
	// ����4�����̋�
	XY around_liberty[4] = { 0 };
	for (int i = 0; i < 4; i++)
	{
		XY xyd = xy + DIR4[i];

		if (board[xyd] == G_OFFBOARD)
		{
			offboard_num++;
			continue;
		}

		if (board[xyd] == G_NONE)
		{
			empty_num++;
			around_liberty[i] = 1;
			continue;
		}

		// �אڂ��鎩���̐F�̘A�̌ċz�_���Ȃ��Ȃ�Ȃ���
		if (groups[board[xyd]].color == color && groups[board[xyd]].liberty_num >= 2)
		{
			alive_num++;
		}
	}

	// ���E��
	if (capture_num == 0 && empty_num == 0 && alive_num == 0)
	{
		return ILLIGAL;
	}
	// �R�E
	if (xy == ko)
	{
		return KO;
	}
	// ��
	if (offboard_num + alive_num == 4 && fill_eye_err)
	{
		return EYE;
	}

	return SUCCESS;
}

void Board::move_legal(const XY xy, const Color color)
{
	pre_changed_group_num = 0;
	pre_removed_group_num = 0;

	// �p�X�̏ꍇ
	if (xy == PASS) {
		ko = -1;
		push_pre_xy(PASS);
		return;
	}

	int tmp_ko;

	// ����4�����̘A
	int around_group_self[4];
	int around_group_self_num = 0;
	int around_group_capture[4];
	int around_group_capture_num = 0;
	int around_group_oponnent[4];
	int around_group_oponnent_num = 0;

	int capture_num = 0;

	// �ċz�_����v����A���擾
	for (GroupIndex idx = groups.begin(); idx != groups.end(); idx = groups.next(idx))
	{
		Group& group = groups[idx];
		if (group.hit_liberties(xy))
		{
			if (group.color == color)
			{
				around_group_self[around_group_self_num++] = idx;
			}
			else {
				// ��邱�Ƃ��ł��邩
				if (group.liberty_num == 1)
				{
					around_group_capture[around_group_capture_num++] = idx;
					capture_num += group.stone_num;
					if (group.stone_num == 1)
					{
						tmp_ko = group.stone[0];
					}
				}
				else {
					around_group_oponnent[around_group_oponnent_num++] = idx;
				}
			}
		}
	}

	// ����4�����̋�
	XY around_liberty[4] = { 0 };
	for (int i = 0; i < 4; i++)
	{
		XY xyd = xy + DIR4[i];

		if (board[xyd] == G_OFFBOARD)
		{
			continue;
		}

		if (board[xyd] == G_NONE)
		{
			around_liberty[i] = 1;
			continue;
		}
	}

	// �΂�u��
	if (around_group_self_num == 0)
	{
		// �A��ǉ�
		int idx = add_group(xy, color, around_liberty);

		// �A�ԍ��𖄂߂�
		board[xy] = idx;

		// �΂̐������Z
		stone_num[color]++;

		// �ύX�����A�ɒǉ�
		pre_changed_group[pre_changed_group_num++] = idx;

		// �󔒍폜
		empty_list.remove(xy);
	}
	else
	{
		// �A�ɐ΂�ǉ�
		Group& group0 = groups[around_group_self[0]];
		group0.add_stone_and_liberties(xy, around_liberty);

		// �A�ԍ��𖄂߂�
		board[xy] = around_group_self[0];

		// �΂̐������Z
		stone_num[color]++;

		// �ύX�����A�ɒǉ�
		pre_changed_group[pre_changed_group_num++] = around_group_self[0];

		// �󔒍폜
		empty_list.remove(xy);

		for (int i = 1; i < around_group_self_num; i++)
		{
			Group& groupi = groups[around_group_self[i]];

			// �A���Ȃ���
			group0.chain_group(xy, groupi);

			// �A�ԍ���u��������
			for (int j = 0; j < groupi.stone_num; j++)
			{
				board[groupi.stone[j]] = around_group_self[0];
			}

			// �A���폜
			remove_group(around_group_self[i]);

			// �ύX�����A�ɒǉ�
			//pre_changed_group[pre_changed_group_num++] = around_group_self[i]; // �Ȃ����A�Ɋ܂܂�邽�ߕs�v
		}
	}

	// �΂����
	for (int i = 0; i < around_group_capture_num; i++)
	{
		// �A�̂������ʒu���󔒂ɂ��Čċz�_��ǉ�����
		Group &remove = groups[around_group_capture[i]];
		for (int j = 0; j < remove.stone_num; j++)
		{
			// �A�̂������ʒu���󔒂ɂ���
			XY xyr = remove.stone[j];
			board[xyr] = G_NONE;

			// �󔒒ǉ�
			empty_list.add(xyr);

			// �אڂ���΂Ɍċz�_��ǉ�
			for (XY d : DIR4)
			{
				XY xyd = xyr + d;
				if (board[xyd] != G_OFFBOARD && board[xyd] != G_NONE)
				{
					if (groups[board[xyd]].color == color)
					{
						groups[board[xyd]].add_liberty(xyr);
					}
				}
			}
		}

		// ����̐΂̐������Z
		stone_num[opponent(color)] -= remove.stone_num;

		// �אڂ���G�̘A����A�ԍ����폜
		for (int j = 0, idx_offset = 0; j < remove.adjacent.get_part_size(); j++, idx_offset += BIT)
		{
			unsigned long idx_tmp;
			while (remove.adjacent.bit_scan_forward(j, &idx_tmp))
			{
				GroupIndex idx = idx_offset + idx_tmp;
				Group& adjacent_group = groups[idx];
				adjacent_group.adjacent.bit_test_and_reset(around_group_capture[i]);

				remove.adjacent.bit_reset_for_bsf(j); // �폜����A�Ȃ̂ōX�V���Ă悢

				// �ύX�����A�ɒǉ�
				pre_changed_group[pre_changed_group_num++] = idx;
			}
		}

		// �A���폜
		remove_group(around_group_capture[i]);

		// �ύX�����A�ɒǉ�
		pre_removed_group[pre_removed_group_num++] = around_group_capture[i];
	}

	// �אڂ���G�̘A�ɂ���
	for (int i = 0; i < around_group_oponnent_num; i++)
	{
		// �ċz�_�̍폜
		groups[around_group_oponnent[i]].remove_liberty(xy);

		// �ύX�����A�ɒǉ�
		if (groups[around_group_oponnent[i]].liberty_num < 3)
		{
			pre_changed_group[pre_changed_group_num++] = around_group_oponnent[i];
		}

		// �אڂ���G�̘A�ԍ���ǉ�
		groups[board[xy]].adjacent.bit_test_and_set(around_group_oponnent[i]);

		// �G�̘A�ɂ�������אڂ���A�Ƃ��Ēǉ�
		groups[around_group_oponnent[i]].adjacent.bit_test_and_set(board[xy]);
	}

	// �p�^�[���l�X�V
	for (int i = 0; i < pre_changed_group_num; i++)
	{
		Group& changed_group = groups[pre_changed_group[i]];
		changed_group.pattern_val = changed_group.color | get_pattern_liberty_val(changed_group.liberty_num);
	}

	// �R�E
	if (capture_num == 1 && groups[board[xy]].stone_num == 1 && groups[board[xy]].liberty_num == 1)
	{
		ko = tmp_ko;
	}
	else {
		ko = -1;
	}

	push_pre_xy(xy);
}

// �A�^�������������擾
int Board::get_atari_save(const Color color, BitBoard<BOARD_BYTE_MAX>& atari_save) const
{
	int atari_save_num = 0;
	atari_save.set_all_zero();

	// �����̐F�̘A�̈ꗗ����ċz�_��1�̏ꏊ�ɂ���
	for (GroupIndex idx = groups.begin(); idx != groups.end(); idx = groups.next(idx))
	{
		const Group& group = groups[idx];

		// �ċz�_�ɑł����ꍇ�ɏ����邱�Ƃ��ł��邩
		if (group.color == color && group.liberty_num == 1)
		{
			// �ċz�_�̏ꏊ
			XY xy = group.get_first_liberty();

			int liberty_num = 0;
			for (XY d : DIR4)
			{
				XY xyd = xy + d;
				if (is_empty(xyd))
				{
					liberty_num++;
					continue;
				}
				if (is_offboard(xyd))
				{
					continue;
				}

				const Group& adjacent_group = get_group(xyd);
				if (adjacent_group.color == color)
				{
					if (board[xyd] != idx)
					{
						// �A����̌ċz�_
						liberty_num += adjacent_group.liberty_num - 1;
					}
				}
				else
				{
					if (adjacent_group.liberty_num == 1)
					{
						// ��邱�Ƃ��ł���
						liberty_num++;
					}
				}
			}
			if (liberty_num >= 3)
			{
				// �A�^������������ɂ��ċz�_��3�ȏ゠��(2�̓V�`���E�̉\��������̂ŏ��O)
				atari_save.bit_test_and_set(xy);
			}

			// �אڂ���A�̌ċz�_��1�̏ꍇ�����邱�Ƃ��ł���
			GroupIndex group_idx_tmp = 0;
			for (int j = 0; j < group.adjacent.get_part_size(); j++, group_idx_tmp += BIT)
			{
				BitBoardPart adjacent_bitborad = group.adjacent.get_bitboard_part(j);
				unsigned long idx;
				while (bit_scan_forward(&idx, adjacent_bitborad))
				{
					GroupIndex group_idx = group_idx_tmp + idx;
					if (groups[group_idx].liberty_num == 1)
					{
						// �ċz�_�̏ꏊ
						atari_save.bit_test_and_set(groups[group_idx].get_first_liberty());
					}

					bit_test_and_reset(&adjacent_bitborad, idx);
				}
			}
		}
	}
	return atari_save_num;
}

// �V�`���E����
bool Board::ladder_search(const Board& board, const Color color, const XY xy, Color tmp_board[BOARD_BYTE_MAX], XY liberties[2], const int depth)
{
	if (depth == 0)
	{
		return false;
	}

	// �΂�u��
	tmp_board[xy] = color;

	// �΂�u�����ꏊ�̌ċz�_�ɂ���
	for (int i = 0; i < 2; i++)
	{
		XY xyl = liberties[i];

		// ��������̏ꏊ�ɓG�̐΂�u��
		tmp_board[liberties[1 - i]] = opponent(color);

		// �G�̐΂�u������̂�������̌ċz�_�̌ċz�_
		int liberty_num = 0;
		XY liberties_afer[8] = { 0 }; // �傫�߂Ɋm��
		for (XY d : DIR4)
		{
			XY xyld = xyl + d;

			if (xyld == xy)
			{
				continue;
			}

			if (board.is_empty(xyld) && tmp_board[xyld] == EMPTY)
			{
				liberties_afer[liberty_num++] = xyld;
				continue;
			}

			if (board.is_offboard(xyld))
			{
				continue;
			}

			// ��������̌ċz�_�ɐ΂�u�����ꍇ�A�΂���邱�Ƃ��ł��邩
			const Group& group = board.get_group(xyld);
			if (board[xyld] == opponent(color))
			{
				if (group.liberty_num == 1)
				{
					liberties_afer[liberty_num++] = xyld;
				}
			}
			else if (board[xyld] == color)
			{
				// �A��������̌ċz�_

				XY liberties_tmp[4];
				int num = group.get_four_liberties(liberties_tmp);
				for (int j = 0; j < num && liberty_num < 4; j++)
				{
					XY xy_tmp = liberties_tmp[j];
					if (tmp_board[xy_tmp] == EMPTY && xy_tmp != xyl)
					{
						liberties_afer[liberty_num++] = xy_tmp;
					}
				}
			}
		}

		// �ċz�_��1�̏ꍇ�A�V�`���E
		if (liberty_num == 1)
		{
			return true;
		}
		else if (liberty_num == 2)
		{
			// �ċz�_��2�̏ꍇ�A���̎��T��
			if (ladder_search(board, color, xyl, tmp_board, liberties_afer, depth - 1))
			{
				return true;
			}
		}

		// ���̏ꏊ�̓V�`���E�ł͂Ȃ�
		// ���߂�
		tmp_board[liberties[1 - i]] = EMPTY;
	}

	// ���߂�
	tmp_board[xy] = EMPTY;

	return false;
}

// �A�^����������肩�i�V�`���E����L�j
bool Board::is_atari_save_with_ladder_search(const Color color, const XY xy) const
{
	if (xy == PASS)
	{
		return false;
	}

	XY liberties[8] = { 0 }; // �傫�߂Ɋm��

	bool is_atari_save = false;

	int liberty_num = 0;
	for (XY d : DIR4)
	{
		XY xyd = xy + d;

		if (is_empty(xyd))
		{
			liberties[liberty_num++] = xyd;
			continue;
		}
		if (is_offboard(xyd))
		{
			continue;
		}

		const Group& adjacent_group = get_group(xyd);

		if (adjacent_group.color == color)
		{
			// �אڂ��鎩���̘A�̌ċz�_��1��
			if (adjacent_group.liberty_num == 1)
			{
				// �A�^�����������
				is_atari_save = true;
			}
			else
			{
				// �A����̌ċz�_

				// �V�`���E������s���ꍇ�A�A����̌ċz�_���擾
				XY liberties_tmp[4];
				int num = adjacent_group.get_four_liberties(liberties_tmp);
				for (int j = 0; j < num && liberty_num < 4; j++)
				{
					XY xy_tmp = liberties_tmp[j];
					if (xy_tmp != xy)
					{
						liberties[liberty_num++] = xy_tmp;
					}
				}
			}
		}
		else
		{
			if (adjacent_group.liberty_num == 1)
			{
				// ��邱�Ƃ��ł���
				liberties[liberty_num++] = xyd;

				// ���A�ɗאڂ��鎩���̘A�̌ċz�_1�̏ꍇ�A�A�^�����������
				GroupIndex group_idx_tmp = 0;
				for (int j = 0; j < adjacent_group.adjacent.get_part_size(); j++, group_idx_tmp += BIT)
				{
					BitBoardPart adjacent_bitborad = adjacent_group.adjacent.get_bitboard_part(j);
					unsigned long idx;
					while (bit_scan_forward(&idx, adjacent_bitborad))
					{
						GroupIndex group_idx = group_idx_tmp + idx;
						if (groups[group_idx].liberty_num == 1)
						{
							return true;
						}

						bit_test_and_reset(&adjacent_bitborad, idx);
					}
				}
			}
		}
	}

	if (is_atari_save)
	{
		if (liberty_num >= 3)
		{
			// �A�^������������ɂ��ċz�_��3�ȏ゠��(2�̓V�`���E�̉\��������̂ŏ��O)
			return true;
		}
		else if (liberty_num == 2)
		{
			// �V�`���E����
			Color tmp_board[BOARD_BYTE_MAX] = { 0 };
			if (!ladder_search(*this, color, xy, tmp_board, liberties, 10))
			{
				// �V�`���E�ł͂Ȃ��̂ŁA�A�^�����������Ƃ���
				return true;
			}
		}
	}

	return false;
}

// �A�^���ɂȂ�肩
bool Board::is_self_atari(const Color color, const XY xy) const
{
	// 1��ł�����Ɍċz�_��1�ɂȂ邩
	int liberty_num = 0;
	int capture_num = 0;
	for (XY d : DIR4)
	{
		XY xyd = xy + d;
		if (is_empty(xyd))
		{
			liberty_num++;
			continue;
		}
		if (is_offboard(xyd))
		{
			continue;
		}

		const Group& adjacent_group = get_group(xyd);

		if (adjacent_group.color == color)
		{
			liberty_num += adjacent_group.liberty_num - 1;
		}
		else
		{
			// ��邱�Ƃ��ł��邩
			if (adjacent_group.liberty_num == 1)
			{
				liberty_num++;
				capture_num += adjacent_group.stone_num;
			}
		}
	}

	// �ł�����̌ċz�_��1�ŃR�E�ɂȂ�Ȃ�
	if (liberty_num == 1 && capture_num != 1)
	{
		return true;
	}

	return false;
}
