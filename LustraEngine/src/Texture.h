#pragma once

#include "Handle.h"
#include "LustraVulkan.h"

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

		// TODO: Remove this.
		operator vk::Image() const
		{
			return allocation.image;
		}
	};

	void CreateTexture2D(std::string_view name, Handle<Texture2D> textureHandle, const TextureDesc2D& texDesc);

	// Will directly copy into the texture, assuming it is not going to be accesibly from the CPU after.
	void CreateReadOnlyTexture2D(
	    std::string_view name,
	    Handle<Texture2D> textureHandle,
	    TextureDesc2D& texDesc,
	    std::span<const std::byte> imageData
	);
	void DestroyTexture2D(Handle<Texture2D> tex);

	void CreateDepthTexture(std::string_view name, Handle<Texture2D> depthTex, const TextureDesc2D& depthDesc);

	// Agnostic to the underlying usage of the texture. It simply re-uses the same texture desc that was used at
	// creation with new dimensions.
	void ResizeTexture(Handle<Texture2D> tex, uint32_t newWidth, uint32_t newHeight);

	inline TextureDesc2D CreateDepthDesc(uint32_t width, uint32_t height, vk::Format depthFormat)
	{
		return TextureDesc2D{
		    .width  = width,
		    .height = height,
		    .format = depthFormat,
		    .usage  = vk::ImageUsageFlagBits::eDepthStencilAttachment,
		};
	}

	// The handle is not owned by this function so the caller is responsible for adding reference if it is to be saved.
	[[nodiscard]] Handle<Texture2D> GetMissingTexture();

} // namespace Resource
