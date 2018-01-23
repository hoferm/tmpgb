#include <stdio.h>
#include <stdlib.h>

#include "gameboy.h"

#include "cpu.h"
#include "interrupt.h"
#include "error.h"
#include "memory.h"
#include "video.h"

#define STAT 0xFF41
#define SCY 0xFF42
#define SCX 0xFF43
#define LY 0xFF44

/* Sprite options */
#define OBJ_BG_PRIORITY (1<<7)
#define Y_FLIP (1<<6)
#define X_FLIP (1<<5)
#define PALETTE_NR (1<<4)

struct sprite {
	u8 y;
	u8 x;
	u8 tile;
	u8 flags;
	u8 addr;
};

static u8 *vram;

static int bg_palette[4] = { 0, 1, 2, 3 };
static int obj_palette_0[4] = { 0, 1, 2, 3 };
static int obj_palette_1[4] = { 0, 1, 2, 3 };

static u8 lcdc_register;
static int display_enable;

static int ly_count;

static void update_palette(void)
{
	u8 bgp_data = read_memory(0xFF47);
	u8 obj_data_0 = read_memory(0xFF48);
	u8 obj_data_1 = read_memory(0xFF49);

	bg_palette[0] = bgp_data & 0x3;
	bg_palette[1] = (bgp_data >> 2) & 0x3;
	bg_palette[2] = (bgp_data >> 4) & 0x3;
	bg_palette[3] = (bgp_data >> 6) & 0x3;

	obj_palette_0[0] = obj_data_0 & 0x3;
	obj_palette_0[1] = (obj_data_0 >> 2) & 0x3;
	obj_palette_0[2] = (obj_data_0 >> 4) & 0x3;
	obj_palette_0[3] = (obj_data_0 >> 6) & 0x3;

	obj_palette_1[0] = obj_data_1 & 0x3;
	obj_palette_1[1] = (obj_data_1 >> 2) & 0x3;
	obj_palette_1[2] = (obj_data_1 >> 4) & 0x3;
	obj_palette_1[3] = (obj_data_1 >> 6) & 0x3;
}

static u8 extract_color(u8 lsb, u8 msb, int px)
{
	return ((lsb >> px) & 0x1) + (((msb >> px) & 0x1) << 1);
}

static void tile_data(u8 *line, u8 tile_nr, int tline)
{
	int i;
	int start = tile_nr * 16;
	u8 color;
	u8 lsb, msb;
	int offset = 0;

	if (!get_bit(lcdc_register, 4)) {
		offset = 0x800;
	}

	lsb = vram[start + offset + (tline * 2)];
	msb = vram[start + offset + (tline * 2) + 1];

	for (i = 7; i >= 0; i--) {
		color = extract_color(lsb, msb, i);
		if (i == 0)
			line[7] = bg_palette[color];
		else
			line[i % 7] = bg_palette[color];
	}
}

static void draw_tiles(u8 *line, u8 ly)
{
	u8 scy = read_memory(SCY) + ly;
	u8 scx = read_memory(SCX);
	u8 tile_nr;
	int offset = 0x1800 + (scy * (WIDTH / 8));
	int i;

	if (get_bit(lcdc_register, 3))
		offset += 0x400;

	for (i = scx; i < WIDTH / 8; i++) {
		tile_nr = vram[i + offset];
		tile_data(line, tile_nr, ly % 8);
		line = line + 8;
	}
}

static int sprites(struct sprite *sp_array, u8 ly)
{
	struct sprite sp;
	int i;
	const int range = 0xA0;
	int size = 0;

	for (i = 0; i < range; i = i + 4) {
		sp.y = vram[i];
		sp.x = vram[i + 1];
		sp.tile = vram[i + 2];
		sp.flags = vram[i + 3];
		sp.addr = i;

		if (sp.y >= ly && (sp.y - 16) <= ly) {
			sp_array[size] = sp;
			size++;
		}
	}

	return size;
}

static int cmp_sprites(const void *p1, const void *p2)
{
	struct sprite s1 = *(struct sprite *) p1;
	struct sprite s2 = *(struct sprite *) p2;

	if (s1.x < s2.x)
		return -1;
	else if (s1.x > s2.x)
		return 1;
	else if (s1.addr < s2.addr)
		return -1;
	else if (s1.addr > s2.addr)
		return 1;
	else
		return 0;
}

static void draw_sprites(u8 *line, u8 ly)
{
	int offset = 0;
	int i;
	struct sprite sp[40];
	int size;
	int x = 0;

	size = sprites(sp, ly);

	if (size > 10) {
		offset = size - 10;
		size = 10;
	}

	qsort(&sp[offset], size, sizeof(struct sprite), cmp_sprites);
	for (x = 0; x < WIDTH; x++) {
		for (i = offset; i < size; i++) {
			int tmp = x - sp[i].x;
			if (tmp < 0)
				tmp *= -1;
			if (tmp < 8)
				tile_data(line, sp[i].tile, ly % 8);
		}
	}
}

static void draw_line(u8 *line, u8 ly)
{
	draw_tiles(line, ly);
	draw_sprites(line, ly);
}

int draw_scanline(u8 *line)
{
	u8 stat = read_memory(STAT);
	u8 ly = read_memory(LY);
	u8 lyc = read_memory(0xFF45);
	int inc = cpu_cycle() - old_cpu_cycle();

	if (!display_enable)
		return -1;

	ly_count += inc;
	if (ly_count < 456)
		return -1;
	ly_count -= 456;

	if (ly >= 144)
		request_interrupt(INT_VBLANK);
	if (ly == lyc)
		write_memory(STAT, set_bit(stat, 2));

	update_palette();
	draw_line(line, ly);
	write_memory(LY, ly + 1);

	return 0;
}

void update_registers(void)
{
	int tmp;
	lcdc_register = read_memory(0xFF40);
	tmp = get_bit(lcdc_register, 7);

	if (!display_enable && tmp)
		write_memory(LY, 0);

	display_enable = tmp;
}

void init_vram(void)
{
	vram = get_vram();
}
