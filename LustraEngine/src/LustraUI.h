#pragma once

#include "LustraVulkan.h"
#include "SDL3/SDL.h"
#include "imgui.h" // Included here so anywhere Lustra UI is included, imgui also gets included.

#include <string>

namespace Lustra::UI
{
	// A UI panel that holds a name, a draw callback, and the data that is expected to be passed to that callback.
	struct Panel
	{
		std::string name              = {};
		void (*UIDrawCallback)(void*) = nullptr;
		void* panelData               = nullptr;
	};

	void Initialize(void* window);

	void ProcessEvent(SDL_Event* event);

	void NewFrame();
	// Should only be used if no rendering is to be called after starting a new frame.
	void EndFrame();

	// Add a panel to be drawn during the UI render pass.
	void AddPanel(const Panel& panel);

	void RenderAndEndFrame(vk::CommandBuffer commandBuffer, vk::Image image, vk::ImageView view);

	void Destroy();
} // namespace Lustra::UI
