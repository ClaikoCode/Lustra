#include "Texture.h"

#include "Graphics.h"
#include "GraphicsUtils.h"
#include "LustraLib/Assert.h"

#include <algorithm>
#include <unordered_set>

namespace GPUMemoryMan
{
	vk::ResultValue<ImageAllocation> AllocateImage(const vk::ImageCreateInfo& imageInfo)
	{
		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.flags                   = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		allocInfo.usage                   = VMA_MEMORY_USAGE_AUTO;

		ImageAllocation imageAllocation = {};

		const auto result = static_cast<vk::Result>(vmaCreateImage(
		    Graphics::gVmaAllocator,
		    reinterpret_cast<const VkImageCreateInfo*>(&imageInfo),
		    &allocInfo,
		    reinterpret_cast<VkImage*>(&imageAllocation.image),
		    &imageAllocation.vmaAllocation,
		    nullptr
		));

		return vk::ResultValue<ImageAllocation>(result, imageAllocation);
	}

	void FreeImageAllocation(ImageAllocation& imageAllocation)
	{
		if (imageAllocation.image)
		{
			vmaDestroyImage(Graphics::gVmaAllocator, imageAllocation.image, imageAllocation.vmaAllocation);
		}
	}

	void CreateDepthTexture(DepthTexture& depthTexture, uint32_t width, uint32_t height, vk::Format depthFormat)
	{
		VALIDATE(depthTexture.allocation.image == nullptr);

		static const std::unordered_set<vk::Format> sValidDepthFormats = {
		    // Pure depth
		    vk::Format::eD32Sfloat,
		    vk::Format::eD16Unorm,

		    // Depth stencil
		    vk::Format::eD16UnormS8Uint,
		    vk::Format::eD24UnormS8Uint,
		    vk::Format::eD32SfloatS8Uint
		};

		const bool isValidDepthFormat = std::ranges::contains(sValidDepthFormats, depthFormat);
		ENSURE_EX(isValidDepthFormat, "Invalid depth format.");

		const vk::ImageCreateInfo depthCreateInfo = {
		    .imageType     = vk::ImageType::e2D,
		    .format        = depthFormat,
		    .extent        = {.width = width, .height = height, .depth = 1},
		    .mipLevels     = 1,
		    .arrayLayers   = 1,
		    .samples       = vk::SampleCountFlagBits::e1,
		    .tiling        = vk::ImageTiling::eOptimal,
		    .usage         = vk::ImageUsageFlagBits::eDepthStencilAttachment,
		    .initialLayout = vk::ImageLayout::eUndefined
		};

		depthTexture.allocation = AssertVk(AllocateImage(depthCreateInfo));

		const vk::ImageViewCreateInfo depthViewInfo = {
		    .image            = depthTexture.allocation.image,
		    .viewType         = vk::ImageViewType::e2D,
		    .format           = depthCreateInfo.format,
		    .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth, .levelCount = 1, .layerCount = 1}
		};

		depthTexture.view =
		    AssertVk(Graphics::gVkDevice.createImageView(depthViewInfo, Graphics::gAllocationCallbacks));

		depthTexture.width  = depthCreateInfo.extent.width;
		depthTexture.height = depthCreateInfo.extent.height;
	}

	void DestroyDepthTexture(DepthTexture& depthTex)
	{
		if (depthTex.view)
		{
			Graphics::gVkDevice.destroyImageView(depthTex.view, Graphics::gAllocationCallbacks);
			FreeImageAllocation(depthTex.allocation);
		}
	}
} // namespace GPUMemoryMan
