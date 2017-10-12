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
			printf(op_names[opcode], read_memory((*cpu.PC) + i + 1));
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
		if (j % 4 == 0)
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
	} else {
		printf("\n\tUnknown command\n");
		return;
	}
}

void setup_debug(void)
{
	cpu_debug_info(&cpu);
}
