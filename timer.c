#include "gameboy.h"

#include "cpu.h"
#include "interrupt.h"
#include "memory.h"

static u8 *div;
static u8 *tima;
static u8 *tma;
static u8 *tac;

static int cpu_clock;
static int tima_count;
static int div_count;

void update_timer(void)
{
	int inc = cpu_cycle() - old_cpu_cycle();
	div_count += inc;
	tima_count += inc;

	if (div_count >= 256) {
		(*div)++;
		div_count -= 256;
	}

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

	if ((get_bit(*tac, 2)) && (tima_count >= cpu_clock)) {
		if (*tima == 0xFF) {
			*tima = *tma;
			request_interrupt(INT_TIMER);
		} else {
			(*tima)++;
		}

		tima_count -= cpu_clock;
	}
}

void init_timer(void)
{
	div = get_div();
	tima = get_tima();
	tma = get_tma();
	tac = get_tac();
}
