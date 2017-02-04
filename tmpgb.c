#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "error.h"
#include "memory.h"

#define READ_SIZE 0x4000

static void usage(void)
{
	fprintf(stderr, "usage: tmpgb -d <file>");
	exit(EXIT_FAILURE);
}

static void load_rom(const char *rom)
{
	FILE *fp;
	unsigned char buffer[READ_SIZE];
	size_t nread;
	int i = -1;

	fp = fopen(rom, "rb");

	if (fp == NULL) {
		fprintf(stderr, "Could not open %s", rom);
		exit(EXIT_FAILURE);
	}

	while (!feof(fp)) {
		nread = fread(buffer, 1, READ_SIZE, fp);

		if (nread < READ_SIZE) {
			fprintf(stderr, "Read: %ld\n", nread);
			if (ferror(fp)) {
				fprintf(stderr, "Error reading %s", rom);
				exit(EXIT_FAILURE);
			}
			break;
		}
		read_rom(buffer, i);
		i++;
	}

	fclose(fp);
}

static void run(void)
{
	int ret;

	if ((ret = init_memory()) != 0) {
		errnr = ret;
		exit_error();
	}

	init_cpu();

	for (;;) {
		fetch_opcode();
	}
}

static int handle_options(int argc, char **argv)
{
	int ret = 0;

	while (argc > 0) {
		const char *cmd = argv[0];

		if (cmd[0] != '-') {
			ret = 1;
			break;
		}

		if (!strcmp(cmd, "-d")) {

		}
		argv++;
		argc--;
	}

	return ret;
}

int main(int argc, char **argv)
{
	char *rom;
	int res = 0;

	if (argc > 2) {
		argc--;
		argv++;
		res = handle_options(argc, argv);
	}

	if (res != 0)
		usage();

	rom = argv[1];
	load_rom(rom);

	run();

	return 0;
}
