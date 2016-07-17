#include <zlib.h>
#include <fstream>
#include "CNNParam.h"

using namespace std;

shared_ptr<ParamMap> load_cnn_param(const char* filename)
{
	shared_ptr<ParamMap> param_map(new ParamMap());

	ifstream infile(filename, ios_base::in | ios_base::binary);

	while (true)
	{
		// Local file header
		LocalFileHeader lfh;
		infile.read((char*)&lfh, 30);

		if (lfh.local_file_header_signature != 0x04034b50)
		{
			break;
		}

		char file_name[1024];

		infile.read(file_name, lfh.file_name_length);
		file_name[lfh.file_name_length] = '\0';

		infile.seekg(lfh.extra_field_length, ios_base::cur);

		// File data
		unsigned char* file_data = new unsigned char[lfh.compressed_size];
		infile.read((char*)file_data, lfh.compressed_size);

		unsigned char* uncompressed_data = new unsigned char[lfh.uncompressed_size];

		z_stream strm = { 0 };
		inflateInit2(&strm, -MAX_WBITS);

		strm.next_in = file_data;
		strm.avail_in = lfh.compressed_size;
		strm.next_out = uncompressed_data;
		strm.avail_out = lfh.uncompressed_size;
		inflate(&strm, Z_NO_FLUSH);
		inflateEnd(&strm);

		// NPY
		NPY* npy = (NPY*)uncompressed_data;
		float* w = (float*)(uncompressed_data + 10 + npy->header_len);

		param_map->insert({ file_name, w });
	}

	return param_map;
}