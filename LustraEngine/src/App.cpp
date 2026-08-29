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

	Camera mainCam   = {};
	FPSCamera fpsCam = {};

	// Init camera
	{
		uint32_t width;
		uint32_t height;
		m_window.GetExtentInPixels(width, height);

		mainCam.SetAspect(width, height);

		mainCam.position = {0.0f, 0.0f, 5.0f};
		mainCam.ForceLookAt(glm::vec3(0.0f));

		mainCam.Update();

		fpsCam.cam = &mainCam;
	}

	bool shouldQuit  = false;
	bool isMinimized = false;
	while (!shouldQuit)
	{
		static float t          = 0.0f;
		static auto currentTime = std::chrono::steady_clock::now();

		const auto oldtime = currentTime; // Save old time before updating.
		currentTime        = std::chrono::steady_clock::now();

		const float deltaT = std::chrono::duration<float>(currentTime - oldtime).count();
		t += deltaT;
		UNUSED_VAR(t);

		SDL_Event event = {};
		while (SDL_PollEvent(&event))
		{
			Lustra::UI::ProcessEvent(&event);

			if (event.type == SDL_EVENT_QUIT)
			{
				shouldQuit = true;
				continue;
			}

			if (event.type == SDL_EVENT_KEY_DOWN)
			{
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

					// Flushes any pending mouse motion for the window.
					// This allows the delta to be zeroed when first reading.
					SDL_GetRelativeMouseState(nullptr, nullptr);
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

				mainCam.SetAspect(width, height);
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
		Lustra::UI::NewFrame();

		// TODO: Move to some Game::Update() function.
		std::vector<Renderer::ModelInstance> modelInstances = {};
		{
			// Update camera only when relative mode is on.
			if (SDL_GetWindowRelativeMouseMode(reinterpret_cast<SDL_Window*>(m_window.GetWindow())))
			{
				fpsCam.Update(deltaT);

				// Forces mouse to be at center of screen without creating mouse movement event (in relative
				// mode).
				m_window.WarpMouseToMiddle();
			}

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

		// Update buffers and record rendering commands.
		{
			ENSURE(fpsCam.cam != nullptr);
			Renderer::Update(context, modelInstances, *fpsCam.cam);

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
