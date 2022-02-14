#pragma once

#include <stdio.h>

#define XLOG_VERBOSE 0
#if XLOG_VERBOSE 
#define XPRINTF(...) printf(__VA_ARGS__)
#else
#define XPRINTF(...) (0)
#endif