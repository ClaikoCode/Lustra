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

	bool shouldQuit  = false;
	bool isMinimized = false;
	while (!shouldQuit)
	{
		float mouseDeltaX = 0.0f;
		float mouseDeltaY = 0.0f;

		SDL_Event event = {};
		while (SDL_PollEvent(&event))
		{
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

				if (event.key.key == SDLK_R)
				{
					SDL_Window* windowPtr = reinterpret_cast<SDL_Window*>(m_window.GetWindow());

					bool currentMode = SDL_GetWindowRelativeMouseMode(windowPtr);

					SDL_SetWindowRelativeMouseMode(windowPtr, !currentMode);
				}
			}

			if (event.type == SDL_EVENT_MOUSE_MOTION)
			{
				if (!SDL_GetWindowRelativeMouseMode(reinterpret_cast<SDL_Window*>(m_window.GetWindow())))
				{
					float x;
					float y;
					SDL_GetMouseState(&x, &y);
				}
				else
				{
					SDL_GetRelativeMouseState(&mouseDeltaX, &mouseDeltaY);
				}
			}

			if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
			{
				PRINT_DEBUG("Window has resized.");

				uint32_t width;
				uint32_t height;
				m_window.GetExtentInPixels(width, height);

				if (width == 0 || height == 0)
				{
					isMinimized = true;
				}
				else
				{
					Graphics::RecreateSwapchain();
					isMinimized = false;
				}

				m_window.UpdateScaling();
			}
		}

		if (shouldQuit)
		{
			break;
		}

		if (isMinimized)
		{
			continue;
		}

		// === START OF GAME AND RENDER LOOP ===

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
