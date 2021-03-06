#include <stdlib.h>
#include <stdio.h>

#include "gameboy.h"

#include "cpu.h"
#include "interrupt.h"
#include "memory.h"
#include "video.h"

#define OBJ_BG_PRIORITY (1<<7)
#define YFLIP (1<<6)
#define XFLIP (1<<5)
#define PALETTENR (1<<4)

struct sprite {
	u8 y;
	u8 x;
	u8 tilenr;
	u8 flags;
	int addr;
};

enum px_type {
	BG,
	SPRITE,
	WINDOW
};

struct pixel {
	int color;
	enum px_type type;
};

static int clock;
static u8 lcdc;
static u8 ly;
static int bg_map;
static u8 *screen;
static int bg_palette[4] = { 0, 1, 2, 3 };
static int obj_palette_0[4] = { 0, 1, 2, 3 };
static int obj_palette_1[4] = { 0, 1, 2, 3 };

static struct sprite spr[40];
static int spr_height;

static int cmp_sprites(const void *p1, const void *p2)
{
	struct sprite *s1 = (struct sprite *) p1;
	struct sprite *s2 = (struct sprite *) p2;

	if (s1->x < s2->x)
		return -1;
	else if (s1->x > s2->x)
		return 1;
	else if (s1->addr < s2->addr)
		return -1;
	else if (s1->addr > s2->addr)
		return 1;
	else
		return 0;
}

static void oam_search(void)
{
	int i;
	struct sprite sp;
	int spr_size = 0;

	for (i = 0xFE00; i < 0xFEA0; i += 4) {
		u8 y = read_memory(i);
		if (ly <= y && ly > (y - spr_height)) {
			sp.y = read_memory(i);
			sp.x = read_memory(i + 1);
			sp.tilenr = read_memory(i + 2);
			sp.flags = read_memory(i + 3);
			sp.addr = i;

			spr[spr_size] = sp;
			spr_size++;
		}
	}
	qsort(spr, spr_size, sizeof(struct sprite), cmp_sprites);
}

static u8 extract_color(u8 lsb, u8 msb, int px)
{
	px = 7 - px;
	return ((lsb >> px) & 0x1) + (((msb >> px) & 0x1) << 1);
}

static u8 tiledata(u8 tilenr, u8 xoff, u8 yoff, enum px_type type)
{
	int offset = (tilenr * 16) + (2 * yoff);
	int lsb, msb;
	u16 addr = 0x8000 + offset;

	if (type != SPRITE) {
		if (!get_bit(lcdc, 4)) {
			if (tilenr < 128)
				addr = 0x9000 + offset;
			else
				addr = 0x8800 + ((tilenr - 128) * 16) + (2 * yoff);
		}
	}
	lsb = read_memory(addr);
	msb = read_memory(addr + 1);
	return extract_color(lsb, msb, xoff);
}

static int spritedata(int x)
{
	int i, color;
	u8 xoff, yoff;
	const int MAX_SPRITES = 10;

	for (i = 0; i < MAX_SPRITES; i++) {
		if (spr[i].x - 8 <= x && spr[i].x >= (x+1)) {
			xoff = 8 - (spr[i].x - x);
			yoff = (spr_height) - (spr[i].y - ly);
			color = tiledata(spr[i].tilenr, xoff, yoff, SPRITE);
			if (color == 0)
				break;
			if (spr[i].flags & PALETTENR)
				return obj_palette_1[color];
			else
				return obj_palette_0[color];
		}
	}
	return -1;
}

static void pixel_transfer(void)
{
	u8 scy = read_memory(0xFF42);
	u8 scx = read_memory(0xFF43);
	u8 line_offset = ly + scy;
	int offset = bg_map + (scx / 8) + (32 * (line_offset / 8));
	int i;
	int tilenr = read_memory(offset);
	struct pixel px;
	int color;
	u8 yoff = (scy + ly) % 8;

	for (i = 0; i < WIDTH; i++) {
		u8 xoff = (i + scx) % 8;
		if (xoff == 0)
			tilenr = read_memory(offset + (i / 8));

		px.color = tiledata(tilenr, xoff, yoff, BG);
		px.color = bg_palette[px.color];
		px.type = BG;
		if (get_bit(lcdc, 1)) {
			color = spritedata(i);
			if (color >= 0) {
				px.color = color;
				px.type = SPRITE;
			}
		}
		screen[i] = px.color;
	}
}

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

static void update_registers(void)
{
	lcdc = read_memory(0xFF40);
	ly = read_memory(0xFF44);
	spr_height = (get_bit(lcdc, 2)) ? 16 : 8;
	clock += (cpu_cycle() - old_cpu_cycle());
	update_palette();

	if (get_bit(lcdc, 3))
		bg_map = 0x9C00;
	else
		bg_map = 0x9800;
}

static u8 set_statmode(u8 stat, u8 statmode)
{
	stat = (stat & (0xFFU << 2)) + statmode;
	write_stat(stat);
	return stat;
}

static void ly_compare(u8 stat)
{
	u8 lyc = read_memory(0xFF45);
	if (ly == lyc) {
		stat = set_bit(stat, 2);
		if (get_bit(stat, 6)) {
			request_interrupt(INT_LCD);
		}
	} else {
		stat = reset_bit(stat, 2);
	}
	write_stat(stat);
}

int draw(u8 *scr)
{
	u8 stat = read_memory(0xFF41);
	u8 stat_mode = stat & 0x3;
	int ret = 0;
	screen = scr;
	update_registers();

	if (!get_bit(lcdc, 7)) {
		write_ly(0);
		return LCD_OFF;
	}

	switch (stat_mode) {
	/* H-Blank */
	case 0:
		if (clock >= 204) {
			write_ly(ly+1);
			if (ly == 144) {
				stat = set_statmode(stat, 1);
				request_interrupt(INT_VBLANK);
			}
			else {
				stat = set_statmode(stat, 2);
			}
			clock -= 204;
			ly_compare(stat);
		}
		break;
	/* V-Blank */
	case 1:
		if (clock >= 456) {
			ly++;
			write_ly(ly);
			if (ly >= 153) {
				write_ly(0);
				stat = set_statmode(stat, 2);
			}
			clock -= 456;
			ly_compare(stat);
		}
		break;
	/* OAM Search */
	case 2:
		if (clock >= 80) {
			oam_search();
			stat = set_statmode(stat, 3);
			clock -= 80;
		}
		break;
	/* LCD Transfer */
	case 3:
		if (clock >= 172) {
			pixel_transfer();
			stat = set_statmode(stat, 0);
			clock -= 172;
			ret = LCD_DRAWN;
		}
		break;
	}
	return ret;
}
