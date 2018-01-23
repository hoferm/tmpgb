#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gameboy.h"

#include "cpu.h"
#include "debug.h"
#include "display.h"
#include "error.h"
#include "memory.h"
#include "timer.h"

#define READ_SIZE 0x4000

static int debug_enabled;

static void usage(void)
{
	usagef("tmpgb [-d] <file>");
}

static void load_rom(const char *rom)
{
	FILE *fp;
	u8 buffer[READ_SIZE];
	size_t nread;
	int i = -1;

	fp = fopen(rom, "rb");

	if (!fp)
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
	int quit = 0;

	if (init_memory() != 0)
		die("invalid rom");

	init_cpu();

	if (init_sdl() != 0)
		die("Failed to create window");

	init_timer();
	setup_debug();

	while (!quit) {
		quit = handle_event();
		if (debug_enabled) {
			debug();
		} else {
			update_timer();
			update_screen();
			fetch_opcode();
		}
	}
}

static int handle_options(int *argc, char ***argv)
{
	int ret = 0;

	while (*argc > 0) {
		const char *cmd = (*argv)[0];

		if (cmd[0] != '-')
			break;

		if (!strcmp(cmd, "-d"))
			debug_enabled = 1;
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

	rom = argv[0];
	load_rom(rom);
	run();
	close_sdl();

	return 0;
}
