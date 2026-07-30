#pragma once

#include "Handle.h"
#include "LustraVulkan.h"
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
		vk::ImageUsageFlags usage = {}; // TODO:move this out and pass it as an argument in functions instead.
		uint32_t mipLevels        = 1;  // 0 is assumed to be MAX mip levels.
	};

	struct Texture2D : ResourceTag
	{
		detail::ImageAllocation allocation = {};
		vk::ImageView view                 = nullptr;

		TextureDesc2D desc;
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

	void CreateTexture2D(Handle<Texture2D> textureHandle, const TextureDesc2D& texDesc);

	void CreateReadOnlyTexture2D(
	    Handle<Texture2D> textureHandle, TextureDesc2D& texDesc, std::span<const std::byte> imageData
	);
	void DestroyTexture2D(Handle<Texture2D> tex);

	void CreateDepthTexture(Handle<DepthTexture> depthTex, const TextureDesc2D& depthDesc);
	void DestroyDepthTexture(Handle<DepthTexture> depthTex);
	void ResizeDepthTexture(Handle<DepthTexture> depthTex, uint32_t newWidth, uint32_t newHeight);

} // namespace Resource
