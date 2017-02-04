#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

void log_msg(const char *fmt, ...)
{
	FILE *fp;
	va_list ap;

	fp = fopen("log_out", "a");

	if (fp == NULL) {
		fprintf(stderr, "Could not open log file");
		exit(EXIT_FAILURE);
	}
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
	fclose(fp);
}
