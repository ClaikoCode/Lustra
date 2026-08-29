#include "Window.h"

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
	m_windowPtr = SDL_CreateWindow(
	    name,
	    static_cast<int>(width),
	    static_cast<int>(height),
	    SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY
	);

	ASSERT_SDL(m_windowPtr != nullptr, "SDL could not create window");

	UpdateScaling();
}

void Window::DestroyWindow()
{
	if (m_windowPtr != nullptr)
	{
		SDL_Window* windowPtr  = GetWindowPtr();
		std::string windowName = SDL_GetWindowTitle(windowPtr);

		SDL_DestroyWindow(windowPtr);
		m_windowPtr = nullptr;

		PRINT_DEBUG("Destroyed window '{}'.", windowName);
	}
}

SDL_Window* Window::GetWindowPtr() const
{
	return m_windowPtr;
}

void Window::GetExtentInPixels(uint32_t& width, uint32_t& height) const
{
	int w;
	int h;
	ASSERT_SDL(SDL_GetWindowSizeInPixels(GetWindowPtr(), &w, &h), "Cant fetch SDL window size in pixels");

	width  = static_cast<uint32_t>(w);
	height = static_cast<uint32_t>(h);
}

void Window::UpdateScaling()
{
	int windowSizeX;
	int windowSizeY;

	ASSERT_SDL(SDL_GetWindowSize(GetWindowPtr(), &windowSizeX, &windowSizeY), "Cant fetch SDL window size.");

	uint32_t pixelWidth;
	uint32_t pixelHeight;
	GetExtentInPixels(pixelWidth, pixelHeight);

	scalingX = static_cast<float>(pixelWidth) / static_cast<float>(windowSizeX);
	scalingY = static_cast<float>(pixelHeight) / static_cast<float>(windowSizeY);
}

void Window::WarpMouseToMiddle() const
{
	SDL_Window* sdlWindow = GetWindowPtr();

	int windowSizeX;
	int windowSizeY;

	ASSERT_SDL(SDL_GetWindowSize(sdlWindow, &windowSizeX, &windowSizeY), "Cant fetch SDL window size.");

	// Guaranteed to not create a move event when in relative mode (unless compiled with special flag).
	SDL_WarpMouseInWindow(sdlWindow, static_cast<float>(windowSizeX) / 2.0f, static_cast<float>(windowSizeY) / 2.0f);
}

Window::~Window()
{
	DestroyWindow();
}
