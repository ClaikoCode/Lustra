#include "Window.h"

#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "SDL3/SDL.h"

Window::Window(const char* name, uint32_t width, uint32_t height)
{
	InitWindow(name, width, height);
}

void Window::InitWindow(const char* name, uint32_t width, uint32_t height)
{
	if (m_window != nullptr)
	{
		PRINT_ERROR("Cannot initialize a window that has already been initialized previously.");
		return;
	}

	m_window = SDL_CreateWindow(name, width, height, SDL_WINDOW_RESIZABLE);
	ENSURE(m_window != nullptr);
}

void Window::DestroyWindow()
{
	if (m_window != nullptr)
	{
		SDL_DestroyWindow((SDL_Window*)m_window);
		m_window = nullptr;
	}
}

Window::~Window()
{
	DestroyWindow();
}
