#include <stdio.h>
#include <SDL2/SDL.h>

#include "gameboy.h"

#include "error.h"
#include "memory.h"
#include "video.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
static int palette[4] = { 0xEEEEEE, 0xB2B2B2, 0x666666, 0x191919 };

static void clear(void)
{
	SDL_SetRenderDrawColor(renderer, 235, 183, 139, 255);
	SDL_RenderClear(renderer);
}

int init_sdl(void)
{
	int ret = 0;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		ret = -1;
		goto out;
	}

	window = SDL_CreateWindow("tmpgb", 0, 0, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
	if (!window) {
		ret = -1;
		goto out;
	}
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		ret = -1;
		goto out;
	}

	init_vram();

out:
	if (ret == -1)
		errorf(SDL_GetError());
	return ret;
}

static void convert_palette(int *rgb, int hex)
{
	rgb[0] = hex >> 16;
	rgb[1] = (hex >> 8) & 0xFF;
	rgb[2] = hex & 0xFF;
}

void update_screen(void)
{
	u8 line[WIDTH];
	int i;
	int color[3];
	u8 ly = read_memory(0xFF44);
	SDL_Rect r;
	r.y = ly;
	r.w = 1;
	r.h = 1;

	memset(line, 0, WIDTH);

	update_registers();
	draw_scanline(line);

	for (i = 0; i < WIDTH; i++) {
		convert_palette(color, palette[line[i]]);
		r.x = i;
		SDL_SetRenderDrawColor(renderer, color[0], color[1], color[2], 255);
		SDL_RenderFillRect(renderer, &r);
	}
	SDL_RenderPresent(renderer);
}

/* Disable display */
void draw_background(void)
{
	clear();
	SDL_RenderPresent(renderer);
}

void close_sdl(void)
{
	SDL_DestroyRenderer(renderer);
	renderer = NULL;

	SDL_DestroyWindow(window);
	window = NULL;

	SDL_Quit();
}

int handle_event(void)
{
	SDL_Event e;

	if (SDL_PollEvent(&e) != 0) {
		if (e.type == SDL_QUIT) {
			return 1;
		} else if (e.type == SDL_KEYDOWN) {
			switch (e.key.keysym.sym) {

			}
		}
	}

	return 0;
}
