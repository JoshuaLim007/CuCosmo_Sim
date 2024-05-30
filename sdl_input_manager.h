#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H
#include <SDL.h>
#include <stdlib.h>
#include <array>

class SDL_InputManager {
private:
	int xM = 0, yM = 0, pxM = 0, pyM = 0;
	int xP = 0, yP = 0;
	SDL_Event* e;

	bool KeyDown[SDL_NUM_SCANCODES];
	bool KeyUp[SDL_NUM_SCANCODES];
	bool KeyHeld[SDL_NUM_SCANCODES];

	bool mouseDown[256];
	bool mouseUp[256];
	bool mouseHeld[256];
	bool quit;

	void PollInput() {
		quit = false;
		while (SDL_PollEvent(e) != 0) {
			if (e->type == SDL_QUIT) {
				quit = true;
			}
			if (e->type == SDL_MOUSEWHEEL) {
				MouseScrollY = e->wheel.y;
			}
			if (e->type == SDL_MOUSEMOTION) {
				xM = e->motion.x;
				yM = e->motion.y;
			}
			else if (e->button.type == SDL_MOUSEBUTTONDOWN) {
				mouseDown[e->button.button] = true;
				mouseHeld[e->button.button] = true;
				xP = e->button.x;
				yP = e->button.y;
			}
			else if (e->button.type == SDL_MOUSEBUTTONUP) {
				mouseUp[e->button.button] = true;
				mouseHeld[e->button.button] = false;
			}
		}

		const Uint8* state = SDL_GetKeyboardState(NULL);
		for (size_t i = 0; i < SDL_NUM_SCANCODES; i++)
		{
			if (state[i] == 1) {
				if (KeyHeld[i] == false) {
					KeyDown[i] = true;
				}
			}
			else if (state[i] == 0) {
				if (KeyHeld[i] == true) {
					KeyUp[i] = true;
				}
			}
			KeyHeld[i] = state[i];
		}

	}
public:
	float MouseScrollY;

	bool WantQuit() {
		return quit;
	}
	void ResetState() {
		pxM = xM;
		pyM = yM;
		MouseScrollY = 0;
		for (size_t i = 0; i < 256; i++)
		{
			mouseDown[i] = false;
			mouseUp[i] = false;
		}
		for (size_t i = 0; i < SDL_NUM_SCANCODES; i++)
		{
			KeyDown[i] = false;
			KeyUp[i] = false;
		}
	}
	SDL_InputManager() {
		e = new SDL_Event();
		std::fill(std::begin(mouseHeld), std::end(mouseHeld), false);
		std::fill(std::begin(mouseUp), std::end(mouseUp), false);
		std::fill(std::begin(mouseDown), std::end(mouseDown), false);
		std::fill(std::begin(KeyDown), std::end(KeyDown), false);
		std::fill(std::begin(KeyUp), std::end(KeyUp), false);
		std::fill(std::begin(KeyHeld), std::end(KeyHeld), false);
	}
	~SDL_InputManager() {
		free(e);
	}
	void SetState() {
		PollInput();
	}
	bool OnKeyDown(const int& code) {
		return KeyDown[code];
	}
	bool OnKeyUp(const int& code) {
		return KeyUp[code];
	}
	bool OnKey(const int& code) {
		return KeyHeld[code];
	}
	bool OnMouseDown(const Uint8& code) {

		return mouseDown[code];
	}
	bool OnMouseUp(const Uint8& code) {

		return mouseUp[code];
	}
	bool OnMouse(const Uint8& code) {

		return mouseHeld[code];
	}
	void GetMouseInput(int& xMove, int& yMove) {
		xMove = xM - pxM;
		yMove = yM - pyM;
	}
	void GetMouseScreenPosition(int& x, int& y) {
		x = xP;
		y = yP;
	}
};

#endif