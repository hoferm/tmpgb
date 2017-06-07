#include <SDL2/SDL.h>

#include "error.h"
#include "video.h"

static SDL_Window *window;
/* static SDL_Renderer *renderer; */
static SDL_Surface *surface;

int init_sdl(void)
{
	int ret = 0;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		ret = -1;
		goto out;
	}

	window = SDL_CreateWindow("tmpgb", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, 0);

	if (window == NULL) {
		ret = -1;
		goto out;
	}

	/* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED); */

	/* if (renderer == NULL) { */
	/* 	ret = -1; */
	/* 	goto out; */
	/* } */

	surface = SDL_GetWindowSurface(window);
	SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0xFF, 0xFF, 0xFF));
	SDL_UpdateWindowSurface(window);
out:
	if (ret == -1) {
		errnr = SDL_ERR;
	}
	return ret;
}

void update_screen(void)
{
	SDL_Point points[HEIGHT * WIDTH];
}

void close_sdl(void)
{
	SDL_DestroyWindow(window);
	window = NULL;

	/* SDL_DestroyRenderer(renderer); */
	/* renderer = NULL; */

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
