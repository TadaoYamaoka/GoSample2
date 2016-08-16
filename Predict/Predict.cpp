#include <stdio.h>
#include <algorithm>
#include <vector>

#include "..\Board.h"
#include "CNN.h"

using namespace std;

#pragma pack(push, 1)
struct Position
{
	XY xy;
	BitBoard<19 * 19> player_color;
	BitBoard<19 * 19> opponent_color;
};
#pragma pack(pop)

// ã«ñ Çì«Ç›çûÇ›
void load_input_features(const wchar_t* filename, Position* &position, int &size)
{
	FILE *fp = _wfopen(filename, L"rb");
	if (fp == nullptr)
	{
		fprintf(stderr, "%s read error.\n", filename);
		exit(1);
	}

	fseek(fp, 0, SEEK_END);
	int filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	size = filesize / sizeof(Position);

	position = new Position[size];

	vector<int> index;
	for (int i = 0; i < size; i++)
	{
		index.push_back(i);
	}

	for (int i = 0; i < size; i++)
	{
		fread(position + index[i], sizeof(Position), 1, fp);
	}

	fclose(fp);
}

void bitboard_to_array(BitBoard<19 * 19> bitboard, float array[19][19])
{
	for (int i = 0; i < 19 * 19; i++)
	{
		*((float*)array + i) = bitboard.bit_test(i);
	}
}

void prepare_input_data(Position* position, float input_data[feature_num][19][19])
{
	bitboard_to_array(position->player_color, input_data[0]);
	bitboard_to_array(position->opponent_color, input_data[1]);

	// empty
	BitBoard<19 * 19> empty;
	for (int i = 0; i < 6; i++)
	{
		empty.get_bitboard_part(i) = ~(position->player_color.get_bitboard_part(i) | position->opponent_color.get_bitboard_part(i));
	}
	bitboard_to_array(empty, input_data[2]);

	// filled with 1
	for (int y = 0; y < 19; y++)
	{
		for (int x = 0; x < 19; x++)
		{
			input_data[3][y][x] = 1.0f;
		}
	}
}

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2)
	{
		return 1;
	}

	// ã«ñ ÉfÅ[É^ì«Ç›çûÇ›
	Position *position;
	int size;

	load_input_features(argv[1], position, size);

	printf("size = %d\n", size);

	// CNNèÄîı
	CNN cnn;
	cnn.prepare_network(argv[2]);

	// ì¸óÕ
	float srcData[minibatch_size][feature_num][19][19];
	for (int i = 0; i < minibatch_size; i++)
	{
		prepare_input_data(position + i, srcData[i]);
	}

	// èáì`îdåvéZ
	float dstData[minibatch_size][1][19][19];
	cnn.forward(srcData, dstData);

	// èoóÕï\é¶
	for (int i = 0; i < minibatch_size; i++)
	{
		printf("minibatch : %d\n", i);
		for (int y = 0; y < 19; y++)
		{
			for (int x = 0; x < 19; x++)
			{
				printf("%f, ", dstData[i][0][y][x]);
			}
			printf("\n");
		}
	}

	return 0;
}