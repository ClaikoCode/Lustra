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
	if (m_windowPtr != nullptr)
	{
		PRINT_ERROR("Cannot initialize a window that has already been initialized previously.");
		return;
	}

	// Creating SDL window with Vulkan flag automatically load the default Vulkan library using
	// SDL_Vulkan_LoadLibrary() if it has not been called before.
	m_windowPtr = SDL_CreateWindow(name, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

	ASSERT_SDL(m_windowPtr != nullptr, "SDL could not create window");
}

void Window::DestroyWindow()
{
	if (m_windowPtr != nullptr)
	{
		auto* windowPtr        = static_cast<SDL_Window*>(m_windowPtr);
		std::string windowName = SDL_GetWindowTitle(windowPtr);

		SDL_DestroyWindow(windowPtr);
		m_windowPtr = nullptr;

		PRINT_DEBUG("Destroyed window '{}'.", windowName);
	}
}

void* Window::GetWindow() const
{
	return m_windowPtr;
}

void Window::GetExtentInPixels(uint32_t& width, uint32_t& height) const
{
	int w;
	int h;
	ASSERT_SDL(
	    SDL_GetWindowSizeInPixels(reinterpret_cast<SDL_Window*>(m_windowPtr), &w, &h),
	    "Cant fetch SDL window size in pixels"
	);

	width  = static_cast<uint32_t>(w);
	height = static_cast<uint32_t>(h);
}

Window::~Window()
{
	DestroyWindow();
}
