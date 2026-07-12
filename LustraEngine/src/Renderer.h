#pragma once

#include "LustraVulkan.h"
#include "Texture.h"

namespace Renderer
{
	struct FrameResources
	{
		vk::CommandPool commandPool          = nullptr;
		vk::CommandBuffer commandBuffer      = nullptr; // Will be destroyed with command pool
		vk::Semaphore imageAcquiredSemaphore = nullptr; // Binary semaphore
	};

	constexpr uint32_t gMaxFramesInFlight = 2u;

	inline Handle<Resource::DepthTexture> gSceneDepth;
	inline std::array<FrameResources, gMaxFramesInFlight> gFramesInFlight = {};

	inline vk::PipelineLayout gHelloTrianglePipelineLayout;
	inline vk::Pipeline gHelloTrianglePipeline;

	inline vk::Semaphore gTimelineSemaphore;

	inline uint32_t gFrameIndex = 0u;

	void Setup();
	void Destroy();

	void Render();
} // namespace Renderer
