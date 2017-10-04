#include <stdlib.h>

#include <SDL2/SDL.h>

#include "gameboy.h"

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

struct point {
	SDL_Point points[64];
	int color;
};

static u8 *vram;
static u16 max_vram_range = 0x97FF;

static int palette[4] = { 0xFFFFFF, 0xB2B2B2, 0x666666, 0x0 };

static u8 lcdc_register;

static void tile_data(u8 *tile, u8 tile_nr)
{
	int i, j;
	int start = tile_nr * 16;
	int end = start + 16;
	u8 color;
	u8 lsb, msb;
	int offset = 0;

	if (BIT_4(lcdc_register)) {
		max_vram_range = 0x8FFF;
	} else {
		max_vram_range = 0x97FF;
		offset = 0x800;
	}

	for (i = start; i < end; i += 2) {
		lsb = vram[i];
		msb = vram[i + 1];

		for (j = 7; j >= 0; --j) {
			color = ((lsb >> j) & 0x1) + (((msb >> j) & 0x1) << 1);

			if (j == 0)
				*(tile + offset + ((i / 2) * 8) + 7) = color;
			else
				*(tile + offset + ((i / 2) * 8) + (j % 7)) = color;
		}
	}
}

static u8 get_scy(void)
{
	return read_memory(0xFF42);
}

static u8 get_scx(void)
{
	return read_memory(0xFF43);
}

int screen_enabled(void)
{
	return BIT_7(lcdc_register);
}

static void draw_blank(void)
{

}

static void draw_scanline(SDL_Point *pt)
{
	int bg_palette[4] = { 0, 1, 2, 3 };
	u8 scanline = read_memory(0xFF44);
	u8 bgp_data = read_memory(0xFF47);
	u8 scy = get_scy();
	u8 scx = get_scx();

	bg_palette[0] = bgp_data & 0x3;
	bg_palette[1] = (bgp_data >> 2) & 0x3;
	bg_palette[2] = (bgp_data >> 4) & 0x3;
	bg_palette[3] = (bgp_data >> 6) & 0x3;

	if (!screen_enabled()) {
		draw_blank();
	} else {
	}
}

struct point line(SDL_Point *pt)
{
	struct point p;
	p.color = 0;
	draw_scanline(pt);

	return p;
}

static void display_tiles(void)
{
	u8 scy = get_scy();
	u8 scx = get_scx();
	int offsetx = scx / 8;
	int offsety = scy / 8;
	int i, j;
	u8 tile[64];

	for (i = offsetx; i < WIDTH; ++i) {
		for (j = offsety; j < HEIGHT; ++j) {
			tile_data(tile, vram[i+j]);
		}
	}
}

static void update_registers(void)
{
	lcdc_register = read_memory(0xFF40);
}

void init_vram(void)
{
	vram = get_vram();
}
