#include <SDL.h>
#include "SDL_image.h"

#undef main

#include "Timer.h"
#include <iostream>
#include "cuda_kernal.h"
#include "sdl_input_manager.h"
#include <chrono>
#include <string>
#include <direct.h>
#include "linear_quadtree.h";

#define WIDTH 1280
#define HEIGHT 1280

void handle_input(void(*callback)(SDL_Event &e)) {
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		callback(e);
	}
}

void save_texture(const char* file_name, SDL_Renderer* renderer, SDL_Texture* texture) {
	SDL_Texture* target = SDL_GetRenderTarget(renderer);
	SDL_SetRenderTarget(renderer, texture);
	int width, height;
	SDL_QueryTexture(texture, NULL, NULL, &width, &height);
	SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
	SDL_RenderReadPixels(renderer, NULL, surface->format->format, surface->pixels, surface->pitch);
	IMG_SavePNG(surface, file_name);
	SDL_FreeSurface(surface);
	SDL_SetRenderTarget(renderer, target);
}

void create_bht(int maxdepth, Node* tree, Body* bodies, int count) {
	float2 min;
	float2 max;
	Point* points = (Point*)bodies;
	find_bounding_box(points, count, min, max);
	for (size_t i = 0; i < count; i++)
	{
		Point p = points[i];
		insert_point(0, 0, maxdepth, tree, min, max, p);
	}
}
void clear_bht(Node* tree, int depth) {
	int len = get_quad_tree_length(depth);
	memset(tree, 0, sizeof(Node) * len);
}
void delete_bht(Node* tree) {
	delete[] tree;
}

int main() {

	std::cout << "Hello world" << std::endl;

	SDL_Init(SDL_INIT_VIDEO);
	auto window = SDL_CreateWindow("window", 32, 32, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
	auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	auto render_texture = SDL_CreateTexture(renderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_BGR888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

	init_cuda_renderer(WIDTH, HEIGHT, 3);
	constexpr int BodyCount = 1000000;
	Body* bodies = init_nbody(BodyCount, time(NULL), 10, 100, make_float2(0, 0), make_float2(WIDTH, HEIGHT), false);

	int tree_depth = 12;
	int tree_len = get_quad_tree_length(tree_depth);
	Node* tree = create_tree(tree_depth);
	init_bth_gpu(tree_len);

	StartTimer();
	clear_bht(tree, tree_depth);
	create_bht(tree_depth, tree, bodies, BodyCount);
	push_bth_gpu(tree, tree_len);
	EndTimer("Tree creation time: ");
	
	StartTimer();
	auto updated = fetch_updated_bodies();
	EndTimer("Download time: ");

	static bool running = true;
	static float colorScale = 50.0f;
	static float renderScale = 0.5f;
	static float2 renderOffset = make_float2(0,0);
	static bool blur = true;
	SDL_InputManager inputManager;
	int image_count = 0;
	
	constexpr int frame_rate = 60;
	constexpr int total_time = 0;
	constexpr int max_images = total_time * frame_rate;

	std::string temp_render_folder = "D:\\joshu\\Pictures\\ndboy_image_saves\\temp\\";
	int s = mkdir("D:\\joshu\\Pictures\\ndboy_image_saves\\");
	s = mkdir(temp_render_folder.c_str());

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

		float offset_speed = 1;
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
		StartTimer();
		
		tick(.01, colorScale, renderScale, renderOffset);
		render_to_rt(pixels, blur);

		fetch_updated_bodies();

		StartTimer();
		push_bth_gpu(tree, tree_len);
		EndTimer("BTH to GPU: ");


		EndTimer("Calculation time: ");
		SDL_UnlockTexture(render_texture);
		if (image_count < max_images) {
			float progress = (float)image_count / max_images;
			auto result = temp_render_folder + "img_" + std::to_string(image_count) + ".png";
			save_texture(result.c_str(), renderer, render_texture);
			std::cout << "Render Progress: " << progress << std::endl;
		}
		else if(image_count == max_images) {
			fprintf(stderr, "Rendering finished!\n");
		}

		image_count++;
		SDL_RenderCopy(renderer, render_texture, NULL, NULL);
		SDL_RenderPresent(renderer);

		inputManager.ResetState();
	}

	return 0;
}