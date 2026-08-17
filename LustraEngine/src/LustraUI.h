#pragma once

#include "LustraVulkan.h"
#include "SDL3/SDL.h"

namespace Lustra::UI
{
	void Initialize(void* window);

	void ProcessEvent(SDL_Event* event);

	void NewFrame();
	// Should only be used if no rendering is to be called after starting a new frame.
	void EndFrame();

	void RenderAndEndFrame(vk::CommandBuffer commandBuffer, vk::Image image, vk::ImageView view);

	void Destroy();
} // namespace Lustra::UI
