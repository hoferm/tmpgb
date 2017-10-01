#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL2/SDL.h"

#include "gameboy.h"

#include "cpu.h"
#include "error.h"
#include "memory.h"
#include "sdl.h"

#define READ_SIZE 0x4000

static char *logfile;
static int oflag;

static void usage(void)
{
	usagef("tmpgb [-o <file>] <file>");
}

static void load_rom(const char *rom)
{
	FILE *fp;
	unsigned char buffer[READ_SIZE];
	size_t nread;
	int i = -1;

	fp = fopen(rom, "rb");

	if (fp == NULL)
		die_errno("could not open ROM: %s", rom);

	while (!feof(fp)) {
		nread = fread(buffer, 1, READ_SIZE, fp);
		if (nread < READ_SIZE) {
			if (ferror(fp)) {
				fclose(fp);
				die_errno("could not read ROM: %s", rom);
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

	if ((ret = init_memory()) != 0)
		die("invalid rom");

	init_cpu();

	if (init_sdl() != 0)
		die("Failed to create window");

	while (!quit) {
		draw_background();
		quit = handle_event();
		fetch_opcode();
	}
}

static int handle_options(int *argc, char ***argv)
{
	int ret = 0;

	while (*argc > 0) {
		const char *cmd = (*argv)[0];

		if (cmd[0] != '-')
			break;

		if (!strcmp(cmd, "-o")) {
			oflag = 1;
			if (*argc < 3) {
				ret = -1;
				break;
			}

			logfile = (*argv)[1];
			(*argv)++;
			(*argc)--;
		}
		(*argv)++;
		(*argc)--;
	}

	return ret;
}

int main(int argc, char **argv)
{
	char *rom;
	int res = 0;

	if (argc < 2)
		usage();

	argc--;
	argv++;
	res = handle_options(&argc, &argv);

	if (res != 0)
		usage();

	if (!oflag)
		logfile = "log_out";

	rom = argv[0];

	if (log_init(logfile) != 0)
		die_errno("could not open file %s", logfile);

	load_rom(rom);

	run();

	log_close();
	close_sdl();

	return 0;
}
