#include <stdio.h>
#include <string.h>

#include "gameboy.h"

#include "error.h"
#include "memory.h"

#define N_LOGO_OFFSET 0x104

/* Starting adresses */

#define MEM_ROM 0x4000
#define MEM_VRAM 0x8000
#define MEM_RAM 0xA000
#define MEM_WRAM 0xC000

/* Cartridge type address */
#define CART_TYPE 0x147

struct {
	u8 rom[0x4000]; /* 0x0000 - 0x3FFF */
	u8 rom_banks[512][0x4000]; /* 0x4000 - 0x7FFF */
	u8 vram[0x2000]; /* 0x8000 - 0x9FFF */
	u8 ram_banks[16][0x2000]; /* 0xA000 - 0xBFFF */
	u8 wram[0x2000]; /* 0xC000 - 0xDFFF */

	u8 interrupt_enable; /* 0xFFFF - First 5 bits used*/

	int mbc_mode;

	u8 *curr_rom;
	u8 *curr_ram;

} memory;

static int cmp_nintendo_logo(void)
{
	int i;
	int ret = 0;

	u8 nintendo_logo[48] =
	{
		0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
		0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
		0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
		0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
		0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
		0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
	};

	for (i = 0; i < 48; ++i) {
		if (memory.rom[i + N_LOGO_OFFSET] != nintendo_logo[i]) {
			ret = 1;
			break;
		}
	}

	return ret;
}

static int check_complement(void)
{
	int i;
	int sum = 0;

	for (i = 0x134; i < 0x14E; ++i) {
		sum += memory.rom[i];
	}

	sum += 25;

	if ((sum & 0xFF) != 0) {
		return 1;
	}

	return 0;
}

void read_rom(const u8 *buffer)
{
#ifdef DEBUG
	int i;

	for (i = 0x104; i < 0x134; ++i) {
		printf("%.2X\n", memory.rom[i]);
	}
#endif
	memcpy(memory.rom, buffer, 0x4000);
}

void write_memory(u16 address, u8 value)
{
	u16 offset;

	switch (address >> 12) {
	case 0x0:
	case 0x1:
	case 0x2:
	case 0x3:
		memory.rom[address] = value;
		break;
	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
		offset = address % MEM_ROM;

		memory.curr_rom[offset] = value;
		break;
	case 0x8:
	case 0x9:
		offset = address % MEM_VRAM;

		memory.vram[offset] = value;
		break;
	case 0xA:
	case 0xB:
		offset = address % MEM_RAM;

		memory.curr_ram[offset] = value;
		break;
	case 0xC:
	case 0xD:
		offset = address % MEM_WRAM;

		memory.wram[offset] = value;
		break;
	case 0xE:
	case 0xF:
		if (address <= 0xFDFF) {
			offset = address % 0xC000;
			memory.wram[offset] = value;
		} else if (address <= 0xFE9F) {

		} else if (address <= 0xFEFF) {

		} else if (address <= 0xFF7F) {

		} else if (address <= 0xFFFE) {

		} else {
			memory.interrupt_enable = value & 0x01FF;
		}
		break;
	default:
		/* Should never be reached */
		fprintf(stderr, "Invalid address");
	}
}

u8 read_memory(u16 address)
{
	u16 offset;
	u8 ret;

	switch (address >> 12) {
	case 0x0:
	case 0x1:
	case 0x2:
	case 0x3:
		ret = memory.rom[address];
		break;
	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
		offset = address % MEM_ROM;

		ret = memory.curr_rom[offset];
		break;
	case 0x8:
	case 0x9:
		offset = address % MEM_VRAM;

		ret = memory.vram[offset];
		break;
	case 0xA:
	case 0xB:
		offset = address % MEM_RAM;

		ret = memory.curr_ram[offset];
		break;
	case 0xC:
	case 0xD:
		offset = address % MEM_WRAM;

		ret = memory.wram[offset];
		break;
	case 0xE:
	case 0xF:
		if (address <= 0xFDFF) {
			offset = address % 0xC000;
			ret = memory.wram[offset];
		} else if (address <= 0xFE9F) {

		} else if (address <= 0xFEFF) {

		} else if (address <= 0xFF7F) {

		} else if (address <= 0xFFFE) {

		} else {
			ret = memory.interrupt_enable & 0x01FF;
		}
		break;
	default:
		/* Should never be reached */
		fprintf(stderr, "Invalid address");
	}
	return ret;
}

void write_mbc1(u16 addr, u8 value)
{
	switch (addr & 0xE000) {
	case 0x2000:
		break;
	}
}

int init_memory(void)
{
	int ret = 0;

	if (cmp_nintendo_logo() != 0) {
		ret = VALIDATION_ERR;
		goto out;
	}

	if (check_complement() != 0) {
		ret = VALIDATION_ERR;
		goto out;
	}

	write_memory(0xFF05, 0x00);
	write_memory(0xFF06, 0x00);
	write_memory(0xFF07, 0x00);
	write_memory(0xFF10, 0x80);
	write_memory(0xFF11, 0xBF);
	write_memory(0xFF12, 0xF3);
	write_memory(0xFF14, 0xBF);
	write_memory(0xFF16, 0x3F);
	write_memory(0xFF17, 0x00);
	write_memory(0xFF19, 0xBF);
	write_memory(0xFF1A, 0x7F);
	write_memory(0xFF1B, 0xFF);
	write_memory(0xFF1C, 0x9F);
	write_memory(0xFF1E, 0xBF);
	write_memory(0xFF20, 0xFF);
	write_memory(0xFF21, 0x00);
	write_memory(0xFF22, 0x00);
	write_memory(0xFF23, 0xBF);
	write_memory(0xFF24, 0x77);
	write_memory(0xFF25, 0xF3);
	write_memory(0xFF26, 0xF1);
	write_memory(0xFF40, 0x91);
	write_memory(0xFF42, 0x00);
	write_memory(0xFF43, 0x00);
	write_memory(0xFF45, 0x00);
	write_memory(0xFF47, 0xFC);
	write_memory(0xFF48, 0xFF);
	write_memory(0xFF49, 0xFF);
	write_memory(0xFF4A, 0x00);
	write_memory(0xFF4B, 0x00);
	write_memory(0xFFFF, 0x00);

	mode = memory.rom[CART_TYPE];

out:
	return ret;
}
