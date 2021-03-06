#include <stdio.h>
#include <string.h>

#include "gameboy.h"

#include "error.h"
#include "mbc.h"
#include "memory.h"

#define N_LOGO_OFFSET 0x104

/* Memory starting adresses */

#define MEM_ROM 0x4000
#define MEM_VRAM 0x8000
#define MEM_RAM 0xA000
#define MEM_WRAM 0xC000
#define MEM_SPRITE_TABLE 0xFE00
#define MEM_IO_REGISTER 0xFF00
#define MEM_HIGH_RAM 0xFF80

/* Cartridge type address */
#define CART_TYPE 0x147

static int bootrom;

static struct mem {
	u8 bootrom[256];
	u8 rom[0x4000]; /* 0x0000 - 0x3FFF */
	u8 rom_bank[512][0x4000]; /* 0x4000 - 0x7FFF */
	u8 vram[0x2000]; /* 0x8000 - 0x9FFF */
	u8 ram_bank[16][0x2000]; /* 0xA000 - 0xBFFF */
	u8 wram[0x2000]; /* 0xC000 - 0xDFFF */
	u8 sprite_table[0xA0];
	u8 io_reg[0x80];
	u8 hram[0x7E];

	u8 interrupt_enable; /* 0xFFFF - First 5 bits used */

	int mbc_mode;

	u8 selected_rom;
	u8 *curr_rom; /* Pointer to current selected ROM bank. */
	u8 *curr_ram; /* Pointer to current selected RAM bank. */

	u8 ram_enable;

} memory;

static int cmp_nintendo_logo(void)
{
	int i;

	u8 nintendo_logo[48] = {
		0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
		0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
		0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
		0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
		0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
		0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
	};

	for (i = 0; i < 48; ++i)
		if (memory.rom[i + N_LOGO_OFFSET] != nintendo_logo[i])
			return 0;

	return 1;
}

static int check_complement(void)
{
	int i;
	int sum = 0;

	for (i = 0x134; i < 0x14E; ++i)
		sum += memory.rom[i];

	sum += 25;

	if ((sum & 0xFF) != 0)
		return 0;

	return 1;
}

void read_bootrom(const u8 *buffer)
{
	bootrom = 1;
	memcpy(memory.bootrom, buffer, 256);
}

void read_rom(const u8 *buffer, int count)
{
	if (count != -1)
		memcpy(memory.rom_bank[count], buffer, 0x4000);
	else
		memcpy(memory.rom, buffer, 0x4000);

}

static void change_mbc_mode(u8 value)
{
	u8 mbc = value & 0x01;

	if (memory.mbc_mode != mbc) {
		if (mbc == 0) {
			memory.curr_ram = memory.ram_bank[0];
		} else {
			memory.selected_rom &= 0x1F;
			memory.curr_rom = memory.rom_bank[memory.selected_rom];
		}

		memory.mbc_mode = mbc;
	}
}
static void write_io(u16 address, u8 value)
{
	u16 offset = (address - MEM_IO_REGISTER);
	u8 *addr = &memory.io_reg[offset];
	if (address == 0xFF00) {
		*addr = (*addr & 0xCF) + (value & 0x30);
	} else if (address == 0xFF41) {
		*addr = (*addr & 0x07) + (value & 0xF8);
	} else if (address == 0xFF44) {
		*addr = 0;
	} else if (address == 0xFF50) {
		if (value & 0x1)
			*addr |= 0x1;
	} else {
		*addr = value;
	}
}

void write_memory(u16 address, u8 value)
{
	u16 offset;
	u8 bank;

	switch (address >> 12) {
	case 0x0:
	case 0x1:
		memory.ram_enable = enable_ram(value);
		break;
	case 0x2:
	case 0x3:
		bank = select_rom_bank(value);
		memory.selected_rom = bank;
		memory.curr_rom = memory.rom_bank[bank];
		break;
	case 0x4:
	case 0x5:
		bank = select_ram_bank(value);
		if (memory.mbc_mode == 1) {
			memory.curr_ram = memory.ram_bank[bank];
		} else {
			memory.selected_rom += (bank << 5);
			memory.curr_rom = memory.rom_bank[memory.selected_rom];
		}
		break;
	case 0x6:
	case 0x7:
		change_mbc_mode(value);
		break;
	case 0x8:
	case 0x9:
		offset = address - MEM_VRAM;

		memory.vram[offset] = value;
		break;
	case 0xA:
	case 0xB:
		offset = address - MEM_RAM;

		memory.curr_ram[offset] = value;
		break;
	case 0xC:
	case 0xD:
		offset = address - MEM_WRAM;

		memory.wram[offset] = value;
		break;
	case 0xE:
	case 0xF:
		if (address <= 0xFDFF) {
			offset = (address - MEM_WRAM) - 0x2000;
			memory.wram[offset] = value;
		} else if (address <= 0xFE9F) {
			offset = (address - MEM_SPRITE_TABLE);
			memory.sprite_table[offset] = value;
		} else if (address <= 0xFEFF) {
		} else if (address <= 0xFF7F) {
			write_io(address, value);
		} else if (address <= 0xFFFE) {
			offset = (address - MEM_HIGH_RAM);
			memory.hram[offset] = value;
		} else {
			memory.interrupt_enable = value & 0x01FF;
		}
		break;
	default:
		/* Should never be reached */
		fprintf(stderr, "Invalid address: %.2X", address);
	}
}

u8 read_memory(u16 address)
{
	u16 offset;
	u8 ret = 0xFF;

	switch (address >> 12) {
	case 0x0:
		if (address < 0x100) {
			if (!(memory.io_reg[0x50] & 0x1)) {
				ret = memory.bootrom[address];
				break;
			}
		}
		ret = memory.rom[address];
		break;
	case 0x1:
	case 0x2:
	case 0x3:
		ret = memory.rom[address];
		break;
	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
		offset = address - MEM_ROM;

		ret = memory.curr_rom[offset];
		break;
	case 0x8:
	case 0x9:
		offset = address - MEM_VRAM;

		ret = memory.vram[offset];
		break;
	case 0xA:
	case 0xB:
		offset = address - MEM_RAM;

		ret = memory.curr_ram[offset];
		break;
	case 0xC:
	case 0xD:
		offset = address - MEM_WRAM;

		ret = memory.wram[offset];
		break;
	case 0xE:
	case 0xF:
		if (address <= 0xFDFF) {
			offset = address - MEM_WRAM - 0x2000;
			ret = memory.wram[offset];
		} else if (address <= 0xFE9F) {
			offset = address - MEM_SPRITE_TABLE;
			ret = memory.sprite_table[offset];
		} else if (address <= 0xFEFF) {
		} else if (address <= 0xFF7F) {
			offset = address - MEM_IO_REGISTER;
			ret = memory.io_reg[offset];
		} else if (address <= 0xFFFE) {
			offset = address - MEM_HIGH_RAM;
			ret = memory.hram[offset];
		} else {
			ret = memory.interrupt_enable & 0x01FF;
		}
		break;
	default:
		/* Should never be reached */
		fprintf(stderr, "Invalid address %.2X", address);
	}
	return ret;
}

void write_ly(u8 v)
{
	memory.io_reg[0x44] = v;
}

void write_joypad(u8 v)
{
	memory.io_reg[0] = v;
}

void write_stat(u8 v)
{
	memory.io_reg[0x41] = v;
}

int bootrom_loaded(void)
{
	return bootrom;
}

int init_memory(void)
{
	if (!bootrom_loaded()) {
		if (!cmp_nintendo_logo())
			return -1;

		if (!check_complement())
			return -1;

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
	}

	write_stat(0x82);

	write_joypad(0xCF);

	write_memory(0xFF02, 0x00);
	write_memory(0xFF03, 0xFF);

	mode = memory.rom[CART_TYPE];

	memory.selected_rom = 0;
	memory.curr_rom = memory.rom_bank[0];
	memory.curr_ram = memory.ram_bank[0];
	memory.interrupt_enable = 0;

	return 0;
}
