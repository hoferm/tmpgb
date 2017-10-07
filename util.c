#include "gameboy.h"

u8 set_bit(u8 val, int bit)
{
	return val | (1U << bit);
}

u8 reset_bit(u8 val, int bit)
{
	return val & (~(1U << bit));
}

int get_bit(u8 val, int bit)
{
	return (val >> bit) & 1;
}
