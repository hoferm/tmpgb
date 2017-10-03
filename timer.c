#include "gameboy.h"

#include "cpu.h"
#include "memory.h"

static u8 *div;
static u8 *tima;
static u8 *tma;
static u8 *tac;

static int cpu_clock;

void update_timer(void)
{
	u64 cycles = cpu_cycle();

	if (cycles % 256 == 0)
		(*div)++;

	switch (*tac & 0x3) {
	case 0:
		cpu_clock = 1024;
		break;
	case 1:
		cpu_clock = 16;
		break;
	case 2:
		cpu_clock = 64;
		break;
	case 3:
		cpu_clock = 256;
		break;
	}

	if ((BIT_2(*tac)) && (cycles % cpu_clock == 0)) {
		if (*tima == 0xFF)
			*tima = *tma;
		else
			(*tima)++;
	}
}

void init_timer(void)
{
	div = get_div();
	tima = get_tima();
	tma = get_tma();
	tac = get_tac();
}
