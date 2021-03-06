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
#define BROM_SIZE 256

static char *bootrom;

static void usage(void)
{
	usagef("tmpgb [-b <boot-rom>] [-d] <rom>");
}

static void load_bootrom(const char *bootrom)
{
	FILE *fp;
	u8 buffer[BROM_SIZE];
	size_t nread;

	fp = fopen(bootrom, "rb");
	if (!fp)
		die_errno("could not open BOOT ROM");

	nread = fread(buffer, 1, BROM_SIZE, fp);
	if (nread < BROM_SIZE) {
		if (ferror(fp)) {
			fclose(fp);
			die_errno("could not read BOOT ROM");
		}
	}
	read_bootrom(buffer);
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
	setup_debug();

	while (!quit) {
		quit = handle_event();
		if (debug_enabled()) {
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
			enable_debug();
		if (!strcmp(cmd, "-b")) {
			(*argv)++;
			(*argc)--;
			if (*argc < 2)
				return -1;
			bootrom = (*argv)[0];
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

	rom = argv[0];
	if (bootrom)
		load_bootrom(bootrom);
	load_rom(rom);
	run();
	close_sdl();

	return 0;
}
