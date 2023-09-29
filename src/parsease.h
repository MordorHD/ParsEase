#ifndef INCLUDED_PARSEASE_H
#define INCLUDED_PARSEASE_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRLEN(a) (sizeof(a)/sizeof*(a))
#define ASSERT(expr, msg) do { \
	if (expr) \
		break; \
	fprintf(stderr, "%s:%s:%d - %s\n", __FILE__, __FUNCTION__, __LINE__, (msg)); \
	exit(1); \
} while (0)

#include "regex.h"
#include "parser.h"

#endif
