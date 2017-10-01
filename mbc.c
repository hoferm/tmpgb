#include "gameboy.h"
#include "memory.h"

u8 select_rom_bank(u8 value)
{
	u8 ret = value & 0x1F;
	switch (mode) {
	case ROM:
		break;
	case MBC1:
		switch (ret) {
		case 0:
		case 0x20:
		case 0x40:
		case 0x60:
			break;
		default:
			ret -= 1;
		}
	}

	return ret;
}

u8 select_ram_bank(u8 value)
{
	return (value & 0x03);
}

u8 enable_ram(u8 value)
{
	return ((value & 0x0A) != 0x0A) ? 0 : 1;
}
