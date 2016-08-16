#pragma once

#include <stdio.h>

extern FILE* logfp;

void open_logfile(const wchar_t* filename);
