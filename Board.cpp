#include "Board.h"

int BOARD_SIZE = 9;
int BOARD_WIDTH = BOARD_SIZE + 1;
int BOARD_STONE_MAX = BOARD_SIZE * BOARD_SIZE;
int BOARD_MAX = BOARD_WIDTH * (BOARD_SIZE + 2);
double KOMI = 6.5;
XY DIR4[4];

MoveResult Board::move(const XY xy, const Color color, const bool fill_eye_err)
{
	// �p�X�̏ꍇ
	if (xy == PASS) {
		ko = -1;
		pre_xy = PASS;
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
			}
		}

		// �A���폜
		remove_group(around_group_capture[i]);
	}

	// �אڂ���G�̘A�ɂ���
	for (int i = 0; i < around_group_oponnent_num; i++)
	{
		// �ċz�_�̍폜
		groups[around_group_oponnent[i]].remove_liberty(xy);

		// �אڂ���G�̘A�ԍ���ǉ�
		groups[board[xy]].adjacent.bit_test_and_set(around_group_oponnent[i]);

		// �G�̘A�ɂ�������אڂ���A�Ƃ��Ēǉ�
		groups[around_group_oponnent[i]].adjacent.bit_test_and_set(board[xy]);
	}

	// �R�E
	if (capture_num == 1 && groups[board[xy]].stone_num == 1 && groups[board[xy]].liberty_num == 1)
	{
		ko = tmp_ko;
	}
	else {
		ko = -1;
	}

	pre_xy = xy;
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
	// �p�X�̏ꍇ
	if (xy == PASS) {
		ko = -1;
		pre_xy = PASS;
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
			}
		}

		// �A���폜
		remove_group(around_group_capture[i]);
	}

	// �אڂ���G�̘A�ɂ���
	for (int i = 0; i < around_group_oponnent_num; i++)
	{
		// �ċz�_�̍폜
		groups[around_group_oponnent[i]].remove_liberty(xy);

		// �אڂ���G�̘A�ԍ���ǉ�
		groups[board[xy]].adjacent.bit_test_and_set(around_group_oponnent[i]);

		// �G�̘A�ɂ�������אڂ���A�Ƃ��Ēǉ�
		groups[around_group_oponnent[i]].adjacent.bit_test_and_set(board[xy]);
	}

	// �R�E
	if (capture_num == 1 && groups[board[xy]].stone_num == 1 && groups[board[xy]].liberty_num == 1)
	{
		ko = tmp_ko;
	}
	else {
		ko = -1;
	}

	pre_xy = xy;
}