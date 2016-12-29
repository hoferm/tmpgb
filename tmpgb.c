#include <stdio.h>
#include <stdlib.h>

#include "memory.h"

static void usage(void)
{
	fprintf(stderr, "usage: tmpgb <file>");
	exit(EXIT_FAILURE);
}

static void load_rom(const char *rom)
{
	FILE *fp;
	size_t nread;

	fp = fopen(rom, "rb");

	if (fp == NULL) {
		fprintf(stderr, "Could not open %s", rom);
		exit(EXIT_FAILURE);
	}

	nread = fread(memory, 32768, 1, fp);

	if (nread == 0) {
		if (ferror(fp)) {
			fprintf(stderr, "Error reading %s", rom);
			exit(EXIT_FAILURE);
		}
	}

	read_rom();
	fclose(fp);
}

int main(int argc, char **argv)
{
	char *rom;

	if (argc != 2)
		usage();

	rom = argv[1];
	load_rom(rom);

	return 0;
}
