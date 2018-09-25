#include <stdio.h>
#include <string.h>

#include "gameboy.h"

#include "cpu.h"
#include "display.h"
#include "interrupt.h"
#include "memory.h"
#include "timer.h"
#include "opnames.h"

static struct cpu_info cpu;
static int enabled;

static void cursor(void)
{
	printf("at 0x%.4X >> ", *cpu.PC);
}

static void disassemble(int n)
{
	int i, j;
	u8 opcode;
	u16 op_param;
	const char **opname;

	for (i = 0, j = 0; j < n; j++, i++) {
		opcode = read_memory((*cpu.PC) + i);
		opname = op_names;

		if (opcode == 0xCB) {
			opname = op_cbnames;
			i++;
		}

		printf("\t");
		if (strstr(opname[opcode], "JR") != NULL) {
			op_param = *cpu.PC + 2 + (char) read_memory(*cpu.PC + i + 1);
			printf(opname[opcode], op_param);
		} else if (strstr(opname[opcode], "%.4X") != NULL) {
			op_param = read_memory(cpu.PC[0] + i + 1) +
				(read_memory(cpu.PC[0] + i + 2) << 8);
			printf(opname[opcode], op_param);
			i += 2;
		} else if (strstr(opname[opcode], "%.2X") != NULL) {
			printf(opname[opcode], read_memory(cpu.PC[0] + i + 1));
			i++;
		} else {
			printf(opname[opcode]);
		}

		printf("\n");
	}
}

static void step(void)
{
	update_timer();
	update_screen();
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

static void intr_status(void)
{
	u8 ly = read_memory(0xFF44);
	u8 ie = read_memory(0xFFFF);
	u8 ir = read_memory(0xFF0F);
	int ime = get_ime();

	printf("ly: %d\n", ly);
	printf("ime: %d, ie: %d, ir: %d\n", ime, ie, ir);
}

static void regs(void)
{
	printf("clock count: %d\n", cpu_cycle());
	intr_status();
	printf("PC: %.4X, SP: %.4X\n", *cpu.PC, *cpu.SP);
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

static void continue_reg_breakpoint(char reg, unsigned value)
{
	switch (reg) {
	case 'a':
	case 'A':
		while (!(*cpu.A == value))
			step();
		break;
	case 'b':
	case 'B':
		while (!(*cpu.B == value))
			step();
		break;
	case 'c':
	case 'C':
		while (!(*cpu.C == value))
			step();
		break;
	case 'd':
	case 'D':
		while (!(*cpu.D == value))
			step();
		break;
	case 'e':
	case 'E':
		while (!(*cpu.E == value))
			step();
		break;
	case 'h':
	case 'H':
		while (!(*cpu.H == value))
			step();
		break;
	case 'l':
	case 'L':
		while (!(*cpu.L == value))
			step();
		break;
	case 'p':
	case 'P':
		while (!(*cpu.PC == value))
			step();
		break;
	default:
		printf("Unknown register\n");
	}
}

void debug(void)
{
	char cmd[16];
	char tmp[16];
	unsigned param;
	unsigned param2;

	cursor();
	if (fgets(cmd, 16, stdin) == NULL)
		die("debug: could not read from stdin");

	if (!strcmp(cmd, "s\n") || !strcmp(cmd, "\n")) {
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
		char reg;
		if (sscanf(tmp, "%c=0x%X", &reg, &param) >= 2) {
			continue_reg_breakpoint(reg, param);
		} else if (sscanf(tmp, "0x%X=0x%X", &param, &param2) >= 2) {
			while (read_memory(param) != param2)
				step();
		}
	} else if (!strcmp(cmd, "run\n")) {
		enabled = 0;
	} else {
		printf("\tUnknown command\n");
		return;
	}
}

void enable_debug(void)
{
	enabled = 1;
}

int debug_enabled(void)
{
	return enabled;
}

void setup_debug(void)
{
	cpu_debug_info(&cpu);
}
