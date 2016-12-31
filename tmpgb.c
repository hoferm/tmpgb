#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "memory.h"

#define READ_SIZE 16384

static void usage(void)
{
	fprintf(stderr, "usage: tmpgb <file>");
	exit(EXIT_FAILURE);
}

static void load_rom(const char *rom)
{
	FILE *fp;
	unsigned char *buffer[READ_SIZE];
	size_t nread;

	fp = fopen(rom, "rb");

	if (fp == NULL) {
		fprintf(stderr, "Could not open %s", rom);
		exit(EXIT_FAILURE);
	}

	nread = fread(buffer, READ_SIZE, 1, fp);

	if (nread == 0) {
		if (ferror(fp)) {
			fprintf(stderr, "Error reading %s", rom);
			exit(EXIT_FAILURE);
		}
	}

	fclose(fp);
	read_rom(buffer);
}

static void run(void)
{
	int ret;

	if ((ret = init_memory()) != 0) {
		errnr = ret;
		exit_error();
	}
}

int main(int argc, char **argv)
{
	char *rom;

	if (argc != 2)
		usage();

	rom = argv[1];
	load_rom(rom);

	run();

	return 0;
}
