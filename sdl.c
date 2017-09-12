#include <stdio.h>
#include "SDL2/SDL.h"

#include "error.h"
#include "video.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
/* static SDL_Texture *texture; */

static void clear(void)
{
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderClear(renderer);
}

int init_sdl(void)
{
	int ret = 0;
	/* SDL_Surface *surface; */

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		ret = -1;
		goto out;
	}

	if (SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer) < 0) {
		ret = -1;
		goto out;
	}

	/* surface = SDL_GetWindowSurface(window); */
	/* if (surface == NULL) { */
	/* 	ret = -1; */
	/* 	goto out; */
	/* } */

	/* texture = SDL_CreateTextureFromSurface(renderer, surface); */
	/* if (texture == NULL) { */
	/* 	ret = -1; */
	/* 	goto out; */
	/* } */

	/* SDL_FreeSurface(surface); */
out:
	if (ret == -1) {
		errnr = SDL_ERR;
		fprintf(stderr, "%s", SDL_GetError());
	}
	return ret;
}

void update_screen(void)
{
}

void draw_background(void)
{
	clear();
	SDL_RenderPresent(renderer);
}

void close_sdl(void)
{
	/* SDL_DestroyTexture(texture); */
	/* texture = NULL; */

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
