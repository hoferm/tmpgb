#include <stdio.h>
#include <string.h>

#include "gameboy.h"

#include "cpu.h"
#include "memory.h"
#include "opnames.h"

static struct cpu_info cpu;

static void cursor(void)
{
	printf("    at 0x%.4X >> ", *cpu.PC);
}

static void disassemble(int n)
{
	int i;
	u8 opcode;

	for (i = 0; i < n; i++) {
		opcode = read_memory((*cpu.PC) + i);
		printf("\t0x%.4X (%s)\n", (*cpu.PC) + i, op_names[opcode]);
	}
}

static void step(void)
{
	disassemble(1);
	fetch_opcode();
}

void debug(void)
{
	char buf[16];
	char cmd[16];
	int param;
	int n;

	cursor();
	if (fgets(buf, 16, stdin) == NULL)
		die("debug: could not read from stdin");

	n = sscanf(buf, "%s", cmd);

	if (n < 1) {
	}

	if (!strcmp(cmd, "s")) {
		step();
	} else if (sscanf(buf, "%s %d\n", cmd, &param) >= 1) {
		if (!strcmp(cmd, "d")) {
			disassemble(param);
		}
	} else {
		printf("Unknown command\n");
		return;
	}
}

void setup_debug(void)
{
	cpu_debug_info(&cpu);
}
