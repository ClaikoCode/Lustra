#pragma once

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

  private:
	void* m_window = nullptr;
};
