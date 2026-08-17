#include "App.h"

#include "AssetManager.h"
#include "AssetRegistry.h"
#include "Graphics.h"
#include "LustraLib/Logger.h"
#include "LustraUI.h"
#include "ModelImporter.h"
#include "Renderer.h"
#include "Resource.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "SDLAssert.h"

namespace
{
} // namespace

App::App(const char* appName) : m_name(appName)
{
	PRINT_DEBUG("Creating App '{}'.", m_name);

	ASSERT_SDL(SDL_Init(SDL_INIT_VIDEO) == true, "Could not init SDL.");
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
	Lustra::UI::Initialize(m_window.GetWindow());
	AssetManager::Setup();
	Renderer::Setup();

	// TODO: Move to some Game::Init()
	Handle<Resource::Model> modelTest = AssetRegistry::Resolve<Resource::Model>(AssetKeyModelTest);

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
				continue;
			}
		}

		// Start UI frame.
		Lustra::UI::ProcessEvent(&event);
		Lustra::UI::NewFrame();

		// TODO: Move to some Game::Update() function.
		std::vector<Renderer::ModelInstance> modelInstances = {};
		{
			modelInstances.push_back({
			    .modelHandle = modelTest,
			    .worldMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(15.0f)),
			});
		}

		// Start render frame.
		Renderer::RenderContext context = Renderer::BeginFrame();

		if (context.skipToNextFrame)
		{
			Lustra::UI::EndFrame();
			continue;
		}

		// Record rendering commands.
		{
			Renderer::Update(context, modelInstances);

			Renderer::Render(context, modelInstances);

			Lustra::UI::RenderAndEndFrame(
			    Renderer::GetFrameCommandBuffer(context),
			    Graphics::gSwapchain.images[context.imageAcquiredIndex],
			    Graphics::gSwapchain.views[context.imageAcquiredIndex]
			);
		}

		// End, submit, and present
		Renderer::EndFrame(context);
		Renderer::SubmitAndPresent(context);
	}

	Graphics::WaitForDevice();

	Lustra::UI::Destroy();
	Renderer::Destroy();
	AssetManager::Destroy();
	Resource::ClearPoolsGPUMemory();
	Graphics::TearDownVulkan();

	// False means a quit without errors
	return false;
}

void App::CreateWindow(const char* name, uint32_t width, uint32_t height)
{
	m_window.InitWindow(name, width, height);
}
