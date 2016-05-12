#pragma once
#include <map>
#include "UCTParallel.h"
#include "learn/Pattern.h"

using namespace std;

// ÉpÉâÉÅÅ[É^ì«Ç›çûÇ›
extern void load_weight(const wchar_t* filepath);

class UCTPattern : public UCTParallel
{
protected:
	static void search_uct_root(Board& board, const Color color, UCTNode* node, const std::map<UCTNode*, UCTNode*>& copynodemap);
	static int search_uct(Board& board, const Color color, UCTNode* node);
	static UCTNode* select_node_with_ucb(const Board& board, const Color color, UCTNode* node);
	static int playout(Board& board, const Color color);


public:
	virtual XY select_move(Board& board, Color color);

};

