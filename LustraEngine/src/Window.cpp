#include "Window.h"

#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "SDL3/SDL.h"
#include "SDLAssert.h"

#include <string>

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

	ASSERT_SDL(m_window != nullptr, "SDL could not create window");
}

void Window::DestroyWindow()
{
	if (m_window != nullptr)
	{
		SDL_Window* windowPtr = (SDL_Window*)m_window;
		std::string windowName = SDL_GetWindowTitle(windowPtr);

		SDL_DestroyWindow(windowPtr);
		m_window = nullptr;

		PRINT_DEBUG("Destroyed window '{}'.", windowName);
	}
}

Window::~Window()
{
	DestroyWindow();
}
