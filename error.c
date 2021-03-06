#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void vreportf(const char *prefix, const char *err, va_list params)
{
	char msg[512];

	vsnprintf(msg, sizeof(msg), err, params);
	fprintf(stderr, "%s%s\n", prefix, msg);
}

void usagef(const char *err, ...)
{
	va_list params;

	va_start(params, err);
	vreportf("usage: ", err, params);
	va_end(params);

	exit(127);
}

void die_errno(const char *err, ...)
{
	char msg[512];
	va_list params;

	snprintf(msg, sizeof(msg), "%s: %s", err, strerror(errno));
	va_start(params, err);
	vreportf("fatal: ", msg, params);
	va_end(params);

	exit(128);
}

void die(const char *err, ...)
{
	va_list params;

	va_start(params, err);
	vreportf("fatal: ", err, params);
	va_end(params);

	exit(128);
}

void errorf(const char *err, ...)
{
	va_list params;

	va_start(params, err);
	vreportf("error: ", err, params);
	va_end(params);
}
