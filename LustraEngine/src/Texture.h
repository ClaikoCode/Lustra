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

	// Not a resource in itself. Simply used to describe a texture concicely.
	struct TextureDesc2D
	{
		uint32_t width            = UINT32_MAX;
		uint32_t height           = UINT32_MAX;
		vk::Format format         = vk::Format::eUndefined;
		vk::ImageUsageFlags usage = {};
	};

	struct DepthTexture : ResourceTag
	{
		detail::ImageAllocation allocation = {};
		vk::ImageView view                 = nullptr;

		TextureDesc2D desc;

		operator vk::Image() const
		{
			return allocation.image;
		}

		static TextureDesc2D CreateDesc(uint32_t width, uint32_t height, vk::Format depthFormat)
		{
			return TextureDesc2D{
			    .width  = width,
			    .height = height,
			    .format = depthFormat,
			    .usage  = vk::ImageUsageFlagBits::eDepthStencilAttachment,
			};
		}
	};

	void CreateDepthTexture(Handle<DepthTexture> depthTex, const TextureDesc2D& depthDesc);
	void DestroyDepthTexture(Handle<DepthTexture> depthTex);
	void ResizeDepthTexture(Handle<DepthTexture> depthTex, uint32_t newWidth, uint32_t newHeight);

} // namespace Resource
