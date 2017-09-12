#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

FILE *fp;

void log_close(void)
{
	fclose(fp);
}

void log_msg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
}

int log_init(const char *file)
{
	fp = fopen(file, "a");

	if (fp == NULL)
		return -1;

	return 0;
}
