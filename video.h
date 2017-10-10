#ifndef VIDEO_H
#define VIDEO_H

#define WIDTH 160
#define HEIGHT 144

void init_vram(void);

int draw_scanline(unsigned char *line);
void update_registers(void);
#endif
