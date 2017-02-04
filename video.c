#include "memory.h"

static int lcd_enabled = 0;
static int bg_enabled = 0;

void enable_lcd(void)
{
	lcd_enabled = 1;
}

void enable_bg(void)
{
	bg_enabled = 1;
}
