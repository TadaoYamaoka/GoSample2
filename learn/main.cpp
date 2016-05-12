#include <windows.h>
#include <stdio.h>
#include "learn.h"

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2)
	{
		return 1;
	}

	if (wcscmp(argv[1], L"prepare") == 0)
	{
		if (argc < 3)
		{
			return 1;
		}
		prepare_pattern(argv[2]);
	}
	else if (wcscmp(argv[1], L"learn") == 0)
	{
		if (argc < 7)
		{
			return 1;
		}
		int game_num = _wtoi(argv[3]);
		int iteration_num = _wtoi(argv[4]);
		float eta = _wtof(argv[5]);
		float ramda = _wtof(argv[6]);
		learn_pattern(argv[2], game_num, iteration_num, eta, ramda);
	}
	else if (wcscmp(argv[1], L"hash") == 0)
	{
		// seed = 9999999661
		// response pattern collision num = 145
		// nonresponse pattern collision num = 11
		uint64_t seeds[] = { 9999997519llu, 9999997537llu, 9999997543llu, 9999997549llu, 9999997561llu, 9999997589llu, 9999997603llu, 9999997613llu, 9999997619llu, 9999997649llu, 9999997663llu, 9999997691llu, 9999997757llu, 9999997789llu, 9999997793llu, 9999997811llu, 9999997823llu, 9999997859llu, 9999997871llu, 9999997919llu, 9999997921llu, 9999997927llu, 9999997951llu, 9999997961llu, 9999998023llu, 9999998027llu, 9999998083llu, 9999998123llu, 9999998137llu, 9999998147llu, 9999998149llu, 9999998159llu, 9999998191llu, 9999998231llu, 9999998233llu, 9999998237llu, 9999998269llu, 9999998273llu, 9999998311llu, 9999998317llu, 9999998363llu, 9999998377llu, 9999998401llu, 9999998413llu, 9999998419llu, 9999998453llu, 9999998521llu, 9999998549llu, 9999998557llu, 9999998597llu, 9999998599llu, 9999998609llu, 9999998611llu, 9999998633llu, 9999998641llu, 9999998653llu, 9999998711llu, 9999998731llu, 9999998783llu, 9999998821llu, 9999998837llu, 9999998861llu, 9999998867llu, 9999999001llu, 9999999017llu, 9999999019llu, 9999999059llu, 9999999067llu, 9999999089llu, 9999999103llu, 9999999151llu, 9999999157llu, 9999999161llu, 9999999169llu, 9999999241llu, 9999999253llu, 9999999319llu, 9999999337llu, 9999999367llu, 9999999371llu, 9999999379llu, 9999999479llu, 9999999491llu, 9999999511llu, 9999999557llu, 9999999619llu, 9999999631llu, 9999999661llu, 9999999673llu, 9999999679llu, 9999999701llu, 9999999703llu, 9999999707llu, 9999999727llu, 9999999769llu, 9999999781llu, 9999999787llu, 9999999817llu, 9999999833llu, 9999999851llu, 9999999881llu, 9999999929llu, 9999999943llu, 9999999967llu };
		for (auto seed : seeds)
		{
			memset(response_pattern_collision, 0, sizeof(response_pattern_collision));
			memset(nonresponse_pattern_collision, 0, sizeof(nonresponse_pattern_collision));
			printf("seed = %lld\n", seed);
			init_hash_table_and_weight(seed);
			check_hash();
		}
		check_hash();
	}
	else if (wcscmp(argv[1], L"dump") == 0)
	{
		dump_weight();
	}
	else if (wcscmp(argv[1], L"clean") == 0)
	{
		if (argc < 3)
		{
			return 1;
		}
		clean_kifu(argv[2]);
	}
	else if (wcscmp(argv[1], L"init") == 0)
	{
		init_weight();
	}

	return 0;
}
