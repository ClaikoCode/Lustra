#pragma once

#include "LustraVulkan.h"
#include "vma/vk_mem_alloc.h"

#include <cstdint>

namespace GPUMemoryMan
{
	struct ImageAllocation
	{
		VmaAllocation vmaAllocation = nullptr;
		vk::Image image             = nullptr;
	};

	struct DepthTexture
	{
		uint32_t width  = UINT32_MAX;
		uint32_t height = UINT32_MAX;

		ImageAllocation allocation = {};
		vk::ImageView view         = nullptr;

		DepthTexture()                                      = default;
		~DepthTexture()                                     = default;
		DepthTexture(const DepthTexture& other)             = delete;
		DepthTexture(const DepthTexture&& other)            = delete;
		DepthTexture& operator=(const DepthTexture& other)  = delete;
		DepthTexture& operator=(const DepthTexture&& other) = delete;

		operator vk::Image() const
		{
			return allocation.image;
		}
	};

	void CreateDepthTexture(DepthTexture& depthTexture, uint32_t width, uint32_t height, vk::Format depthFormat);
	void DestroyDepthTexture(DepthTexture& depthTex);

} // namespace GPUMemoryMan
