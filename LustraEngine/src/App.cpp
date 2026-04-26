#include "App.h"

#include "Graphics.h"
#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "SDLAssert.h"

namespace
{
} // namespace

App::App(const char* appName) : m_name(appName)
{
	PRINT_DEBUG("Creating App '{}'.", m_name);

	ASSERT_SDL(SDL_Init(SDL_INIT_VIDEO) == true, "Could not init SDL");
	ASSERT_SDL(SDL_Vulkan_LoadLibrary(nullptr) == true, "Could not load Vulkan library.");
}

App::~App()
{
	m_window.DestroyWindow();
	// Make sure this is called after all Vulkan related resources have been freed (including any windows).
	SDL_Vulkan_UnloadLibrary();
	SDL_Quit();

	PRINT_DEBUG("Destroyed App '{}'.", m_name);
}

bool App::RunApp()
{
	Graphics::SetupVulkan(m_name, m_window);

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

			if (event.key.key == SDLK_ESCAPE)
			{
				shouldQuit = true;
			}
		}
	}

	Graphics::TearDownVulkan();

	// False means a quit without errors
	return false;
}

void App::CreateWindow(const char* name, uint32_t width, uint32_t height)
{
	m_window.InitWindow(name, width, height);
}
