#include <SDL.h>
#undef main

#include <iostream>
#include "cuda_kernal.h"
#include "sdl_input_manager.h"

#define WIDTH 1280
#define HEIGHT 1280

void handle_input(void(*callback)(SDL_Event &e)) {
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		callback(e);
	}
}

int main() {

	std::cout << "Hello world" << std::endl;

	SDL_Init(SDL_INIT_VIDEO);
	auto window = SDL_CreateWindow("window", 32, 32, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
	auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	auto render_texture = SDL_CreateTexture(renderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_BGR888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

	init_cuda_renderer(WIDTH, HEIGHT, 3);
	init_nbody(250000, 0, 1, 10, make_float2(0,0), make_float2(WIDTH, HEIGHT), false);

	static bool running = true;
	static float colorScale = 1000.0f;
	static float renderScale = 0.5f;
	static float2 renderOffset = make_float2(0,0);
	static bool blur = true;
	SDL_InputManager inputManager;

	while (running) {
		inputManager.SetState();

		if (inputManager.WantQuit()) {
			running = false;
		}
		if (inputManager.MouseScrollY != 0) {
			if (inputManager.OnKey(SDL_SCANCODE_LCTRL)) {
				if (inputManager.MouseScrollY < 0) {
					colorScale *= 1 / 1.25f;
				}
				else if (inputManager.MouseScrollY > 0) {
					colorScale *= 1.25;
				}

				printf("%f color scale\n", colorScale);
			}
			else {

				if (inputManager.MouseScrollY < 0) {
					renderScale *= 1 / 1.25f;
				}
				else if(inputManager.MouseScrollY > 0) {
					renderScale *= 1.25;
				}

				renderScale = fmaxf(renderScale, 0.0f);
				printf("%f render scale\n", renderScale);
			}
		}

		float offset_speed = 10;
		if (inputManager.OnKey(SDL_SCANCODE_LSHIFT)) {
			offset_speed *= 2;
		}
		if (inputManager.OnKey(SDL_SCANCODE_W)) {
			renderOffset.y -= offset_speed;
		}
		if (inputManager.OnKey(SDL_SCANCODE_A)) {
			renderOffset.x += offset_speed;
		}
		if (inputManager.OnKey(SDL_SCANCODE_D)) {
			renderOffset.x -= offset_speed;
		}
		if (inputManager.OnKey(SDL_SCANCODE_S)) {
			renderOffset.y += offset_speed;
		}
		if (inputManager.OnKeyDown(SDL_SCANCODE_1)) {
			blur = !blur;
		}

		uint8_t* pixels;
		int pitch = 0;
		SDL_LockTexture(render_texture, NULL, (void**)&pixels, &pitch);
		
		clear_rt();
		tick(.01, colorScale, renderScale, renderOffset);
		render_to_rt(pixels, blur);

		SDL_UnlockTexture(render_texture);
	
		SDL_RenderCopy(renderer, render_texture, NULL, NULL);
		SDL_RenderPresent(renderer);

		inputManager.ResetState();
	}

	return 0;
}