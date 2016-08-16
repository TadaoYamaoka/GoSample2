#include "log.h"

FILE* logfp = stdout;

void open_logfile(const wchar_t* filename)
{
	logfp = _wfopen(filename, L"a");
	if (!logfp)
	{
		fprintf(stderr, "log file open error.\n");
		return;
	}
}