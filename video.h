#ifndef VIDEO_H
#define VIDEO_H

#define WIDTH 160
#define HEIGHT 144

enum screen_status {
	LCD_OFF = 1,
	LCD_DRAWN = 2
};

int draw(u8 *screen);
#endif
