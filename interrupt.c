#include "gameboy.h"

#include "interrupt.h"
#include "memory.h"

#define MEM_IR (0xFF0F)
#define MEM_IE (0xFFFF)

static int interrupt_master_enable = 0;

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
	u8 interrupt_enable;
	u8 interrupt_request;
	int interrupt = 0;
	int tmp;

	if (interrupt_master_enable) {
		interrupt_enable = read_memory(MEM_IE);
		interrupt_request = read_memory(MEM_IR);
		tmp = interrupt_enable & interrupt_request;

		if (!tmp)
			return interrupt;

		if (get_bit(tmp, 0)) {
			interrupt_request = reset_bit(interrupt_request, 0);
			interrupt = INT_VBLANK;
		} else if (get_bit(tmp, 1)) {
			interrupt_request = reset_bit(interrupt_request, 1);
			interrupt = INT_LCD;
		} else if (get_bit(tmp, 2)) {
			interrupt_request = reset_bit(interrupt_request, 2);
			interrupt = INT_TIMER;
		} else if (get_bit(tmp, 3)) {
			interrupt_request = reset_bit(interrupt_request, 3);
			interrupt = INT_SERIAL;
		} else if (get_bit(tmp, 4)) {
			interrupt_request = reset_bit(interrupt_request, 4);
			interrupt = INT_JOYPAD;
		}

		write_memory(MEM_IR, interrupt_request);
		interrupt_master_enable = 0;
	}

	return interrupt;
}

void request_interrupt(int interrupt)
{
	u8 interrupt_request = read_memory(MEM_IR);
	u8 interrupt_enable = read_memory(MEM_IE);
	switch (interrupt) {
	case INT_VBLANK:
		write_memory(MEM_IE, set_bit(interrupt_enable, 0));
		write_memory(MEM_IR, set_bit(interrupt_request, 0));
		break;
	case INT_LCD:
		write_memory(MEM_IE, set_bit(interrupt_enable, 1));
		write_memory(MEM_IR, set_bit(interrupt_request, 1));
		break;
	case INT_TIMER:
		write_memory(MEM_IE, set_bit(interrupt_enable, 2));
		write_memory(MEM_IR, set_bit(interrupt_request, 2));
		break;
	case INT_SERIAL:
		write_memory(MEM_IE, set_bit(interrupt_enable, 3));
		write_memory(MEM_IR, set_bit(interrupt_request, 3));
		break;
	case INT_JOYPAD:
		write_memory(MEM_IE, set_bit(interrupt_enable, 4));
		write_memory(MEM_IR, set_bit(interrupt_request, 4));
		break;
	}
}
