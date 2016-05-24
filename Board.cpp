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

	// パスの場合
	if (xy == PASS) {
		ko = -1;
		push_pre_xy(PASS);
		return SUCCESS;
	}

	int tmp_ko;

	// 周囲4方向の連
	int around_group_self[4];
	int around_group_self_num = 0;
	int around_group_capture[4];
	int around_group_capture_num = 0;
	int around_group_oponnent[4];
	int around_group_oponnent_num = 0;

	int capture_num = 0;

	// 呼吸点が一致する連を取得
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
				// 取ることができるか
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
	// 周囲4方向の空白
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

		// 隣接する自分の色の連の呼吸点がなくならないか
		if (groups[board[xyd]].color == color && groups[board[xyd]].liberty_num >= 2)
		{
			alive_num++;
		}
	}

	// 自殺手
	if (capture_num == 0 && empty_num == 0 && alive_num == 0)
	{
		return ILLIGAL;
	}
	// コウ
	if (xy == ko)
	{
		return KO;
	}
	// 眼
	if (offboard_num + alive_num == 4 && fill_eye_err)
	{
		return EYE;
	}

	// 石を置く
	if (around_group_self_num == 0)
	{
		// 連を追加
		int idx = add_group(xy, color, around_liberty);

		// 連番号を埋める
		board[xy] = idx;

		// 石の数を加算
		stone_num[color]++;

		// 変更した連に追加
		pre_changed_group[pre_changed_group_num++] = idx;

		// 空白削除
		empty_list.remove(xy);
	}
	else
	{
		// 連に石を追加
		Group& group0 = groups[around_group_self[0]];
		group0.add_stone_and_liberties(xy, around_liberty);

		// 連番号を埋める
		board[xy] = around_group_self[0];

		// 石の数を加算
		stone_num[color]++;

		// 変更した連に追加
		pre_changed_group[pre_changed_group_num++] = around_group_self[0];

		// 空白削除
		empty_list.remove(xy);

		for (int i = 1; i < around_group_self_num; i++)
		{
			Group& groupi = groups[around_group_self[i]];

			// 連をつなげる
			group0.chain_group(xy, groupi);

			// 連番号を置き換える
			for (int j = 0; j < groupi.stone_num; j++)
			{
				board[groupi.stone[j]] = around_group_self[0];
			}

			// 連を削除
			remove_group(around_group_self[i]);

			// 変更した連に追加
			//pre_changed_group[pre_changed_group_num++] = around_group_self[i]; // つなげた連に含まれるため不要
		}
	}

	// 石を取る
	for (int i = 0; i < around_group_capture_num; i++)
	{
		// 連のあった位置を空白にして呼吸点を追加する
		Group &remove = groups[around_group_capture[i]];
		for (int j = 0; j < remove.stone_num; j++)
		{
			// 連のあった位置を空白にする
			XY xyr = remove.stone[j];
			board[xyr] = G_NONE;

			// 空白追加
			empty_list.add(xyr);

			// 隣接する石に呼吸点を追加
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

		// 相手の石の数を減算
		stone_num[opponent(color)] -= remove.stone_num;

		// 隣接する敵の連から連番号を削除
		for (int j = 0, idx_offset = 0; j < remove.adjacent.get_part_size(); j++, idx_offset += BIT)
		{
			unsigned long idx_tmp;
			while (remove.adjacent.bit_scan_forward(j, &idx_tmp))
			{
				GroupIndex idx = idx_offset + idx_tmp;
				Group& adjacent_group = groups[idx];
				adjacent_group.adjacent.bit_test_and_reset(around_group_capture[i]);

				remove.adjacent.bit_reset_for_bsf(j); // 削除する連なので更新してよい

				// 変更した連に追加
				pre_changed_group[pre_changed_group_num++] = idx;
			}
		}

		// 連を削除
		remove_group(around_group_capture[i]);

		// 変更した連に追加
		pre_removed_group[pre_removed_group_num++] = around_group_capture[i];
	}

	// 隣接する敵の連について
	for (int i = 0; i < around_group_oponnent_num; i++)
	{
		// 呼吸点の削除
		groups[around_group_oponnent[i]].remove_liberty(xy);

		// 変更した連に追加
		if (groups[around_group_oponnent[i]].liberty_num < 3)
		{
			pre_changed_group[pre_changed_group_num++] = around_group_oponnent[i];
		}

		// 隣接する敵の連番号を追加
		groups[board[xy]].adjacent.bit_test_and_set(around_group_oponnent[i]);

		// 敵の連にも自分を隣接する連として追加
		groups[around_group_oponnent[i]].adjacent.bit_test_and_set(board[xy]);
	}

	// パターン値更新
	for (int i = 0; i < pre_changed_group_num; i++)
	{
		Group& changed_group = groups[pre_changed_group[i]];
		changed_group.pattern_val = changed_group.color | get_pattern_liberty_val(changed_group.liberty_num);
	}

	// コウ
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
	// 周囲4方向の連
	int capture_num = 0;

	// 呼吸点が一致する連を取得
	for (GroupIndex idx = groups.begin(); idx != groups.end(); idx = groups.next(idx))
	{
		const Group& group = groups[idx];
		if (group.hit_liberties(xy))
		{
			if (group.color == color)
			{
			}
			else {
				// 取ることができるか
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
	// 周囲4方向の空白
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

		// 隣接する自分の色の連の呼吸点がなくならないか
		if (groups[board[xyd]].color == color && groups[board[xyd]].liberty_num >= 2)
		{
			alive_num++;
		}
	}

	// 自殺手
	if (capture_num == 0 && empty_num == 0 && alive_num == 0)
	{
		return ILLIGAL;
	}
	// コウ
	if (xy == ko)
	{
		return KO;
	}
	// 眼
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

	// パスの場合
	if (xy == PASS) {
		ko = -1;
		push_pre_xy(PASS);
		return;
	}

	int tmp_ko;

	// 周囲4方向の連
	int around_group_self[4];
	int around_group_self_num = 0;
	int around_group_capture[4];
	int around_group_capture_num = 0;
	int around_group_oponnent[4];
	int around_group_oponnent_num = 0;

	int capture_num = 0;

	// 呼吸点が一致する連を取得
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
				// 取ることができるか
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

	// 周囲4方向の空白
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

	// 石を置く
	if (around_group_self_num == 0)
	{
		// 連を追加
		int idx = add_group(xy, color, around_liberty);

		// 連番号を埋める
		board[xy] = idx;

		// 石の数を加算
		stone_num[color]++;

		// 変更した連に追加
		pre_changed_group[pre_changed_group_num++] = idx;

		// 空白削除
		empty_list.remove(xy);
	}
	else
	{
		// 連に石を追加
		Group& group0 = groups[around_group_self[0]];
		group0.add_stone_and_liberties(xy, around_liberty);

		// 連番号を埋める
		board[xy] = around_group_self[0];

		// 石の数を加算
		stone_num[color]++;

		// 変更した連に追加
		pre_changed_group[pre_changed_group_num++] = around_group_self[0];

		// 空白削除
		empty_list.remove(xy);

		for (int i = 1; i < around_group_self_num; i++)
		{
			Group& groupi = groups[around_group_self[i]];

			// 連をつなげる
			group0.chain_group(xy, groupi);

			// 連番号を置き換える
			for (int j = 0; j < groupi.stone_num; j++)
			{
				board[groupi.stone[j]] = around_group_self[0];
			}

			// 連を削除
			remove_group(around_group_self[i]);

			// 変更した連に追加
			//pre_changed_group[pre_changed_group_num++] = around_group_self[i]; // つなげた連に含まれるため不要
		}
	}

	// 石を取る
	for (int i = 0; i < around_group_capture_num; i++)
	{
		// 連のあった位置を空白にして呼吸点を追加する
		Group &remove = groups[around_group_capture[i]];
		for (int j = 0; j < remove.stone_num; j++)
		{
			// 連のあった位置を空白にする
			XY xyr = remove.stone[j];
			board[xyr] = G_NONE;

			// 空白追加
			empty_list.add(xyr);

			// 隣接する石に呼吸点を追加
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

		// 相手の石の数を減算
		stone_num[opponent(color)] -= remove.stone_num;

		// 隣接する敵の連から連番号を削除
		for (int j = 0, idx_offset = 0; j < remove.adjacent.get_part_size(); j++, idx_offset += BIT)
		{
			unsigned long idx_tmp;
			while (remove.adjacent.bit_scan_forward(j, &idx_tmp))
			{
				GroupIndex idx = idx_offset + idx_tmp;
				Group& adjacent_group = groups[idx];
				adjacent_group.adjacent.bit_test_and_reset(around_group_capture[i]);

				remove.adjacent.bit_reset_for_bsf(j); // 削除する連なので更新してよい

				// 変更した連に追加
				pre_changed_group[pre_changed_group_num++] = idx;
			}
		}

		// 連を削除
		remove_group(around_group_capture[i]);

		// 変更した連に追加
		pre_removed_group[pre_removed_group_num++] = around_group_capture[i];
	}

	// 隣接する敵の連について
	for (int i = 0; i < around_group_oponnent_num; i++)
	{
		// 呼吸点の削除
		groups[around_group_oponnent[i]].remove_liberty(xy);

		// 変更した連に追加
		if (groups[around_group_oponnent[i]].liberty_num < 3)
		{
			pre_changed_group[pre_changed_group_num++] = around_group_oponnent[i];
		}

		// 隣接する敵の連番号を追加
		groups[board[xy]].adjacent.bit_test_and_set(around_group_oponnent[i]);

		// 敵の連にも自分を隣接する連として追加
		groups[around_group_oponnent[i]].adjacent.bit_test_and_set(board[xy]);
	}

	// パターン値更新
	for (int i = 0; i < pre_changed_group_num; i++)
	{
		Group& changed_group = groups[pre_changed_group[i]];
		changed_group.pattern_val = changed_group.color | get_pattern_liberty_val(changed_group.liberty_num);
	}

	// コウ
	if (capture_num == 1 && groups[board[xy]].stone_num == 1 && groups[board[xy]].liberty_num == 1)
	{
		ko = tmp_ko;
	}
	else {
		ko = -1;
	}

	push_pre_xy(xy);
}

// アタリを助ける手を取得
int Board::get_atari_save(const Color color, BitBoard<BOARD_BYTE_MAX>& atari_save) const
{
	int atari_save_num = 0;
	atari_save.set_all_zero();

	// 自分の色の連の一覧から呼吸点が1の場所について
	for (GroupIndex idx = groups.begin(); idx != groups.end(); idx = groups.next(idx))
	{
		const Group& group = groups[idx];

		// 呼吸点に打った場合に助けることができるか
		if (group.color == color && group.liberty_num == 1)
		{
			// 呼吸点の場所
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
						// 連結後の呼吸点
						liberty_num += adjacent_group.liberty_num - 1;
					}
				}
				else
				{
					if (adjacent_group.liberty_num == 1)
					{
						// 取ることができる
						liberty_num++;
					}
				}
			}
			if (liberty_num >= 3)
			{
				// アタリを助けた後にも呼吸点が3以上ある(2はシチョウの可能性があるので除外)
				atari_save.bit_test_and_set(xy);
			}

			// 隣接する連の呼吸点が1の場合助けることができる
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
						// 呼吸点の場所
						atari_save.bit_test_and_set(groups[group_idx].get_first_liberty());
					}

					bit_test_and_reset(&adjacent_bitborad, idx);
				}
			}
		}
	}
	return atari_save_num;
}

// シチョウ判定
bool Board::ladder_search(const Board& board, const Color color, const XY xy, Color tmp_board[BOARD_BYTE_MAX], XY liberties[2], const int depth)
{
	if (depth == 0)
	{
		return false;
	}

	// 石を置く
	tmp_board[xy] = color;

	// 石を置いた場所の呼吸点について
	for (int i = 0; i < 2; i++)
	{
		XY xyl = liberties[i];

		// もう一方の場所に敵の石を置く
		tmp_board[liberties[1 - i]] = opponent(color);

		// 敵の石を置いた後のもう一方の呼吸点の呼吸点
		int liberty_num = 0;
		XY liberties_afer[8] = { 0 }; // 大きめに確保
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

			// もう一方の呼吸点に石を置いた場合、石を取ることができるか
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
				// 連結した後の呼吸点

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

		// 呼吸点が1の場合、シチョウ
		if (liberty_num == 1)
		{
			return true;
		}
		else if (liberty_num == 2)
		{
			// 呼吸点が2の場合、次の手を探索
			if (ladder_search(board, color, xyl, tmp_board, liberties_afer, depth - 1))
			{
				return true;
			}
		}

		// この場所はシチョウではない
		// 手を戻す
		tmp_board[liberties[1 - i]] = EMPTY;
	}

	// 手を戻す
	tmp_board[xy] = EMPTY;

	return false;
}

// アタリを助ける手か（シチョウ判定有）
bool Board::is_atari_save_with_ladder_search(const Color color, const XY xy) const
{
	if (xy == PASS)
	{
		return false;
	}

	XY liberties[8] = { 0 }; // 大きめに確保

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
			// 隣接する自分の連の呼吸点が1か
			if (adjacent_group.liberty_num == 1)
			{
				// アタリを助ける手
				is_atari_save = true;
			}
			else
			{
				// 連結後の呼吸点

				// シチョウ判定を行う場合、連結後の呼吸点を取得
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
				// 取ることができる
				liberties[liberty_num++] = xyd;

				// 取る連に隣接する自分の連の呼吸点1の場合、アタリを助ける手
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
			// アタリを助けた後にも呼吸点が3以上ある(2はシチョウの可能性があるので除外)
			return true;
		}
		else if (liberty_num == 2)
		{
			// シチョウ判定
			Color tmp_board[BOARD_BYTE_MAX] = { 0 };
			if (!ladder_search(*this, color, xy, tmp_board, liberties, 10))
			{
				// シチョウではないので、アタリを助ける手とする
				return true;
			}
		}
	}

	return false;
}

// アタリになる手か
bool Board::is_self_atari(const Color color, const XY xy) const
{
	// 1手打った後に呼吸点が1になるか
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
			// 取ることができるか
			if (adjacent_group.liberty_num == 1)
			{
				liberty_num++;
				capture_num += adjacent_group.stone_num;
			}
		}
	}

	// 打った後の呼吸点が1でコウにならない
	if (liberty_num == 1 && capture_num != 1)
	{
		return true;
	}

	return false;
}
