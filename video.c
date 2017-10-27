#include <stdio.h>
#include <stdlib.h>

#include "gameboy.h"

#include "cpu.h"
#include "interrupt.h"
#include "error.h"
#include "memory.h"
#include "video.h"

#define BG_MAP_SIZE 256
#define BG_MAP_START 0x9800
#define BG_MAP_END 0x9BFF

struct sprite {
	u8 y;
	u8 x;
	u8 tile;
	u8 flags;
};

static u8 *vram;

static int bg_palette[4] = { 0, 1, 2, 3 };

static u8 lcdc_register;
static int display_enable;

static int ly_count;

static void update_palette(void)
{
	u8 bgp_data = read_memory(0xFF47);

	bg_palette[0] = bgp_data & 0x3;
	bg_palette[1] = (bgp_data >> 2) & 0x3;
	bg_palette[2] = (bgp_data >> 4) & 0x3;
	bg_palette[3] = (bgp_data >> 6) & 0x3;
}

static void tile_data(u8 *tile, u8 tile_nr)
{
	int i;
	int start = tile_nr * 16;
	u8 color;
	u8 lsb, msb;
	int offset = 0;

	if (!get_bit(lcdc_register, 4)) {
		offset = 0x800;
	}

	lsb = vram[start + offset];
	msb = vram[start + offset + 1];

	for (i = size; i >= 0; i--) {
		color = ((lsb >> i) & 0x1) + (((msb >> i) & 0x1) << 1);
		if (i == 0)
			*(tile + 7) = bg_palette[color];
		else
			*(tile + (i % 7)) = bg_palette[color];
	}
}

static void draw_tiles(u8 *line, u8 ly)
{
	u8 scy = read_memory(0xFF42) + ly;
	u8 scx = read_memory(0xFF43);
	int i;
	u8 tile_nr;
	int offset = 0x1800 + (scy * (WIDTH / 8));

	if (get_bit(lcdc_register, 3))
		offset += 0x400;

	for (i = scx; i < WIDTH / 8; i++) {
		tile_nr = vram[i + offset];
		tile_data(line, tile_nr);
	}
}

static void draw_sprites(void)
{
	/* TODO */
}

/* TODO: Use enum for return */
int draw_scanline(u8 *line)
{
	u8 stat = read_memory(0xFF41);
	u8 ly = read_memory(0xFF44);
	u8 lyc = read_memory(0xFF45);
	int inc = cpu_cycle() - old_cpu_cycle();

	if (!display_enable)
		return -1;

	ly_count += inc;
	if (ly_count < 252)
		return -1;
	ly_count -= 252;

	if (ly >= 144)
		request_interrupt(INT_VBLANK);
	if (ly == lyc)
		write_memory(0xFF41, set_bit(stat, 2));

	update_palette();
	draw_tiles(line, ly);
	draw_sprites();
	write_memory(0xFF44, ly + 1);

	return 0;
}

void update_registers(void)
{
	int tmp;
	lcdc_register = read_memory(0xFF40);
	tmp = get_bit(lcdc_register, 7);

	if (display_enable == 0 && tmp == 1)
		write_memory(0xFF44, 0);

	display_enable = tmp;
}

void init_vram(void)
{
	vram = get_vram();
}
