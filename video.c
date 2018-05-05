#include <stdlib.h>

#include "gameboy.h"

#include "cpu.h"
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
static int spr_size;
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

	for (i = 0xFF00; i < 0xFFA0; i += 4) {
		u8 y = read_memory(i);
		if (ly < y && ly > (y - spr_height)) {
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
	if (px == 0)
		px = 7;
	else
		px = 7 - px;
	return ((lsb >> px) & 0x1) + (((msb >> px) & 0x1) << 1);
}

static u8 tiledata(u8 tilenr, u8 xoff, u8 yoff)
{
	int offset = tilenr * 16 - 16 + (2 * yoff);
	int lsb, msb;
	u16 addr = 0x8000 + offset;

	if (!get_bit(lcdc, 4)) {
		if (tilenr < 128)
			addr = 0x9000 + offset;
		else
			addr = 0x8800 + offset;
	}
	lsb = read_memory(addr);
	msb = read_memory(addr + 1);
	return extract_color(lsb, msb, xoff);
}

static void pixel_transfer(void)
{
	u8 scy = read_memory(0xFF42);
	u8 scx = read_memory(0xFF43);
	int offset = bg_map + scx / 8 + (WIDTH * (ly + scy)) / 8;
	int i, j;
	int tilenr = read_memory(offset);
	struct pixel px;
	u8 color;

	for (i = 0; i < WIDTH; i++) {
		u8 xoff = i % 8;
		u8 yoff = ly % 8;
		if (i == 0) {
			px.color = tiledata(tilenr, scx % 8, yoff);
			px.color = bg_palette[px.color];
			px.type = BG;
		} else if (xoff == 0) {
			tilenr = read_memory(offset + i / 8);
		}
		px.color = tiledata(tilenr, xoff, yoff);
		px.color = bg_palette[px.color];
		px.type = BG;
		if (!get_bit(lcdc, 1))
			goto loop_end;
		for (j = 0; j < 10; j++) {
			if (spr[j].x - 8 <= (i+1) && spr[j].x >= (i+1)) {
				xoff = 7 - (spr[j].x - i);
				yoff = (spr_height - 1) - (spr[j].y - ly);
				color = tiledata(spr[j].tilenr, xoff, yoff);
				if (color != 0) {
					if (spr[j].flags & PALETTENR)
						px.color = obj_palette_1[color];
					else
						px.color = obj_palette_0[color];
					px.type = SPRITE;
				}
				break;
			}
		}
loop_end:
		*screen = px.color;
		screen++;
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

static void set_statmode(u8 stat, u8 statmode)
{
	stat = (stat & (1U << 2)) + statmode;
	write_memory(0xFF41, stat);
}

int draw(u8 *scr)
{
	u8 stat = read_memory(0xFF41);
	u8 stat_mode = stat & 0x3;
	screen = scr;
	update_registers();

	if (!get_bit(lcdc, 7))
		return LCD_OFF;

	switch (stat_mode) {
	/* H-Blank */
	case 0:
		if (clock >= 204) {
			write_memory(0xFF44, ly+1);
			set_statmode(stat, 1);
			clock -= 204;
		}
		break;
	/* V-Blank */
	case 1:
		if (clock >= 456) {
			ly++;
			write_memory(0xFF44, ly);
			if (ly > 154) {
				write_memory(0xFF44, 1);
				set_statmode(stat, 2);
			}
			clock -= 456;
		}
		break;
	/* OAM Search */
	case 2:
		if (clock >= 80) {
			oam_search();
			set_statmode(stat, 3);
			clock -= 80;
		}
		break;
	/* LCD Transfer */
	case 3:
		if (clock >= 172) {
			pixel_transfer();
			set_statmode(stat, 0);
			clock -= 172;
		}
		break;
	}
	return 0;
}
