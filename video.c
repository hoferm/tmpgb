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

static int palette[4] = { 0xFFFFFF, 0xB2B2B2, 0x666666, 0x0 };
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

/* partial visible tiles? */
static void tile_data(u8 *tile, u8 tile_nr, int size)
{
	int j;
	int start = tile_nr * 16;
	u8 color;
	u8 lsb, msb;
	int offset = 0;

	if (!get_bit(lcdc_register, 4)) {
		offset = 0x800;
	}

	lsb = vram[start];
	msb = vram[start + 1];

	for (j = size; j >= 0; j--) {
		color = ((lsb >> j) & 0x1) + (((msb >> j) & 0x1) << 1);
		if (j == 0)
			*(tile + offset + 7) = palette[bg_palette[color]];
		else
			*(tile + offset + (j % 7)) = palette[bg_palette[color]];
	}
}

/*
 * Draw tiles per line
 * skip tile_nr check after first check
 */
static void draw_tiles(u8 *line, u8 ly)
{
	u8 scy = read_memory(0xFF42) + ly;
	u8 scx = read_memory(0xFF43);
	int i, j = 0;
	int k = 0;
	u8 tile_nr;
	int offset = 0x1800 + (scy * WIDTH);

	if (get_bit(lcdc_register, 3))
		offset += 0x400;

	for (i = scx + 1, tile_nr = *(vram + scx + offset); i < WIDTH; i++) {
		if (tile_nr == *(vram + i + offset)) {
			k++;
			tile_nr = *(vram + i + offset);
			continue;
		}
		tile_data(line + j, tile_nr, k);

		j++;
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
