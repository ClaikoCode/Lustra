#include "App.h"

#include "Graphics.h"
#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "SDLAssert.h"

namespace
{
	std::vector<const char*> GetSDLInstanceExtensions()
	{
		uint32_t sdlInstanceExtensionsCount      = 0;
		const char* const* sdlInstanceExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlInstanceExtensionsCount);
		ASSERT_SDL(sdlInstanceExtensions != nullptr, "Unable to get Vulkan instance extensions required by SDL");

		std::vector<const char*> externalRequestedExtensions(sdlInstanceExtensionsCount);
		for (uint32_t i = 0; i < sdlInstanceExtensionsCount; i++)
		{
			externalRequestedExtensions[i] = sdlInstanceExtensions[i];
		}

		return externalRequestedExtensions;
	}
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
	Graphics::SetupVulkan(m_name, GetSDLInstanceExtensions());

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

	Graphics::TearDownVulkan();

	// False means a quit without errors
	return false;
}

void App::CreateWindow(const char* name, uint32_t width, uint32_t height)
{
	m_window.InitWindow(name, width, height);
}
