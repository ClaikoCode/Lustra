#include "App.h"

#include "LustraLib/Logger.h"
#include "SDL3/SDL.h"

App::App(const char* appName) : m_name(appName)
{
	PRINT_DEBUG("Creating App '{}'", m_name);

	SDL_Init(SDL_INIT_VIDEO);
}

App::~App()
{
	PRINT_DEBUG("Destroying App '{}'", m_name);

	m_window.DestroyWindow();

	SDL_Quit();
}

bool App::RunApp()
{
	SDL_Event event = {};
	bool shouldQuit = false;
	while (!shouldQuit)
	{
		SDL_PollEvent(&event);
		if (event.type == SDL_EVENT_QUIT)
		{
			shouldQuit = true;
			continue;
		}

		if (event.type == SDL_EVENT_KEY_DOWN)
		{
			PRINT_LOG("Key {} was pressed!", SDL_GetKeyName(event.key.key));
		}
	}

	return false;
}

void App::CreateWindow(const char* name, uint32_t width, uint32_t height)
{
	m_window.InitWindow(name, width, height);
}
