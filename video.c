#include "gameboy.h"

#include "memory.h"

/* LCD COONTROL flags */
#define BIT_7(reg) (reg & 0x80)
#define BIT_6(reg) (reg & 0x40)
#define BIT_5(reg) (reg & 0x20)
#define BIT_4(reg) (reg & 0x10)
#define BIT_3(reg) (reg & 0x08)
#define BIT_2(reg) (reg & 0x04)
#define BIT_1(reg) (reg & 0x02)
#define BIT_0(reg) (reg & 0x01)

#define WIDTH 160
#define HEIGHT 144

#define BGM_SIZE 256

static u8 *vram;

static void get_tile_data(int pattern)
{
	*vram = get_vram();
	u8 lcdc_register = read_memory(0xFF40);
	u16 max_range = 0x97FF;

	if (BIT_4(lcdc_register)) {
		max_range = 0x8FFF;
	}
}

static void display_tiles(void)
{

}

static u8 read_register(u16 address)
{
	u8 value = read_memory(address);

	return value;
}
