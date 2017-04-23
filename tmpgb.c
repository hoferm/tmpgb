#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "error.h"
#include "memory.h"
#include "sdl.h"

#define READ_SIZE 0x4000

static void usage(void)
{
	fprintf(stderr, "usage: tmpgb -d <file>\n");
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
		fprintf(stderr, "Could not open %s\n", rom);
		exit(EXIT_FAILURE);
	}

	while (!feof(fp)) {
		nread = fread(buffer, 1, READ_SIZE, fp);

		if (nread < READ_SIZE) {
			if (ferror(fp)) {
				fprintf(stderr, "Error reading %s\n", rom);
				fclose(fp);
				exit(EXIT_FAILURE);
			}
		}
		read_rom(buffer, i);
		i++;
	}

	fclose(fp);
}

static void run(void)
{
	int ret;
	int quit = 0;

	if ((ret = init_memory()) != 0) {
		errnr = ret;
		exit_error();
	}

	init_cpu();

	if (init_sdl() != 0) {
		fprintf(stderr, "Failed to create window\n");
		exit(EXIT_FAILURE);
	}

	while (!quit) {
		quit = handle_event();
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

	if (argc < 2) 
		usage();

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

	close_sdl();

	return 0;
}
