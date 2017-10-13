#include <stdio.h>
#include <string.h>

#include "gameboy.h"

#include "cpu.h"
#include "memory.h"
#include "opnames.h"

static struct cpu_info cpu;

static void cursor(void)
{
	printf("at 0x%.4X >> ", *cpu.PC);
}

static void disassemble(int n)
{
	int i, j;
	u8 opcode;
	u16 op_param;

	for (i = 0, j = 0; j < n; j++, i++) {
		opcode = read_memory((*cpu.PC) + i);

		printf("\t");
		if (strstr(op_names[opcode], "%.4X") != NULL) {
			op_param = read_memory(cpu.PC[0] + i + 1) +
				(read_memory(cpu.PC[0] + i + 2) << 8);
			printf(op_names[opcode], op_param);
			i += 2;
		} else if (strstr(op_names[opcode], "%.2X") != NULL) {
			printf(op_names[opcode], read_memory(cpu.PC[0] + i + 1));
			i++;
		} else {
			printf(op_names[opcode]);
		}

		printf("\n");
	}
}

static void step(void)
{
	disassemble(1);
	fetch_opcode();
}

static void flags(void)
{
	u8 z;
	u8 n;
	u8 h;
	u8 c;

	printf("\t");

	z = get_bit(*cpu.F, 7);
	n = get_bit(*cpu.F, 6);
	h = get_bit(*cpu.F, 5);
	c = get_bit(*cpu.F, 4);

	printf("Z: %d, N: %d, H: %d, C: %d\n", z, n, h, c);
}

static void regs(void)
{
	flags();
	printf("\tB: %.2X, C: %.2X\n", *cpu.B, *cpu.C);
	printf("\tD: %.2X, E: %.2X\n", *cpu.D, *cpu.E);
	printf("\tH: %.2X, L: %.2X\n", *cpu.H, *cpu.L);
	printf("\tA: %.2X\n", *cpu.A);
}

static void view_mem(u16 start, u16 end)
{
	int i;
	int j = 0;

	if (start > end) {
		i = end;
		end = start;
		start = i;
	}

	for (i = start; i <= end; i++) {
		if (j % 4 == 0 && j != 0)
			printf("\n");
		printf("\t0x%.4X: %.2X", i, read_memory(i));
		j++;
	}
	printf("\n");
}

void debug(void)
{
	char cmd[16];
	char tmp[16];
	unsigned param;
	/* unsigned param2; */

	cursor();
	if (fgets(cmd, 16, stdin) == NULL)
		die("debug: could not read from stdin");

	if (!strcmp(cmd, "s\n")) {
		step();
	} else if (sscanf(cmd, "d %ud\n", &param) >= 1) {
		disassemble(param);
	} else if (sscanf(cmd, "m 0x%X\n", &param) >= 1) {
		view_mem(param, param);
	} else if (sscanf(cmd, "m %s\n", tmp) >= 1) {
		if (!strcmp(tmp, "io"))
			view_mem(0xFF00, 0xFF7F);
		else if (!strcmp(tmp, "sprite") || !strcmp(tmp, "oam"))
			view_mem(0xFE00, 0xFE9F);
		else if (!strcmp(tmp, "hram"))
			view_mem(0xFF80, 0xFFFE);
		else if (!strcmp(tmp, "vram"))
			view_mem(0x8000, 0x9FFF);
	} else if (!strcmp(cmd, "f\n")) {
		flags();
	} else if (!strcmp(cmd, "reg\n")) {
		regs();
	} else if (sscanf(cmd, "if %s\n", tmp) >= 1) {
		if (!strcmp(tmp, "B = 0") || !strcmp(tmp, "B=0")) {
			while (!*cpu.B == 0)
				step();
		}
	} else {
		printf("\tUnknown command\n");
		return;
	}
}

void setup_debug(void)
{
	cpu_debug_info(&cpu);
}
