#pragma once

#include "SDL3/SDL_video.h" // For SDL_Window

#include <cstdint>

class Window
{
  public:
	Window() = default;
	Window(const char* name, uint32_t width, uint32_t height);
	~Window();

	// Cannot copy window object.
	Window(Window& other)            = delete;
	Window(Window&& temp)            = delete;
	Window& operator=(Window& other) = delete;
	Window& operator=(Window&& temp) = delete;

	void InitWindow(const char* name, uint32_t width, uint32_t height);
	void DestroyWindow();

	// Gets a NON CONST ptr to the window.
	// This is allowed to use as const as there might be window-related function calls that expect non const pointers
	// even though they do not expect to change any data.
	SDL_Window* GetWindowPtr() const;

	void GetExtentInPixels(uint32_t& width, uint32_t& height) const;
	void UpdateScaling();

	void WarpMouseToMiddle() const;

	// === PUBLIC VARS ===

	// Has to be updated if window resizes.
	float scalingX = 1.0f;
	float scalingY = 1.0f;

  private:
	SDL_Window* m_windowPtr = nullptr;
};
