#pragma once

#include "LustraVulkan.h"
#include "Resource.h"
#include "vma/vk_mem_alloc.h"

#include <cstdint>

namespace detail
{
	struct ImageAllocation
	{
		VmaAllocation vmaAllocation = nullptr;
		vk::Image image             = nullptr;
	};
} // namespace detail

namespace Resource
{
	struct DepthTexture : ResourceTag
	{
		uint32_t width  = UINT32_MAX;
		uint32_t height = UINT32_MAX;

		detail::ImageAllocation allocation = {};
		vk::ImageView view                 = nullptr;

		operator vk::Image() const
		{
			return allocation.image;
		}
	};

	Handle<DepthTexture> CreateDepthTexture(uint32_t width, uint32_t height, vk::Format depthFormat);
	void DestroyDepthTexture(Handle<DepthTexture> depthTex);

} // namespace Resource
