#pragma once

#include <map>
#include <memory>
#include <string>

using namespace std;

typedef map<string, float*> ParamMap;
shared_ptr<ParamMap> load_cnn_param(const wchar_t* filename);

#pragma pack(push, 1)
struct LocalFileHeader
{
	unsigned long local_file_header_signature; // 4_bytes (0x04034b50)
	unsigned short version_needed_to_extract; // 2_bytes
	unsigned short general_purpose_bit_flag; // 2_bytes
	unsigned short compression_method; // 2_bytes
	unsigned short last_mod_file_time; // 2_bytes
	unsigned short last_mod_file_date; // 2_bytes
	unsigned long crc_32; // 4_bytes
	unsigned long compressed_size; // 4_bytes
	unsigned long uncompressed_size; // 4_bytes
	unsigned short file_name_length; // 2_bytes
	unsigned short extra_field_length; // 2_bytes
									   // ‚±‚±‚Ü‚Å30bytes
};

struct NPY
{
	char magic_string[6]; // 6 bytes (0x93NUMPY)
	unsigned char major_version; // 1 byte
	unsigned char minor_version; // 1 byte
	unsigned short header_len; // 2 bytes
							   // ‚±‚±‚Ü‚Å10bytes
};
#pragma pack(pop)
