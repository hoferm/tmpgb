#include "gameboy.h"

#include "interrupt.h"
#include "memory.h"

static int interrupt_master_enable = 1;

void set_ime(int enabled)
{
	interrupt_master_enable = enabled;
}

int get_ime(void)
{
	return interrupt_master_enable;
}

int execute_interrupt(void)
{
	int interrupt_enable;
	int interrupt_request;
	int interrupt = 0;

	if (interrupt_master_enable) {
		interrupt_enable = read_memory(0xFFFF);
		interrupt_request = read_memory(0xFF0F);

		if (interrupt_enable && interrupt_request) {
			if (BIT_0(interrupt_enable) && BIT_0(interrupt_request)) {
				interrupt_request = RESET_BIT_0(interrupt_request);
				interrupt = INT_VBLANK;
			} else if (BIT_1(interrupt_enable) && BIT_1(interrupt_request)) {
				interrupt_request = RESET_BIT_1(interrupt_request);
				interrupt = INT_LCD;
			} else if (BIT_2(interrupt_enable) && BIT_2(interrupt_request)) {
				interrupt_request = RESET_BIT_2(interrupt_request);
				interrupt = INT_TIMER;
			} else if (BIT_3(interrupt_enable) && BIT_3(interrupt_request)) {
				interrupt_request = RESET_BIT_3(interrupt_request);
				interrupt = INT_SERIAL;
			} else if (BIT_4(interrupt_enable) && BIT_4(interrupt_request)) {
				interrupt_request = RESET_BIT_4(interrupt_request);
				interrupt = INT_JOYPAD;
			} else {
				return interrupt;
			}

			write_memory(0xFF0F, interrupt_request);
			interrupt_master_enable = 0;
		}
	}

	return interrupt;
}
