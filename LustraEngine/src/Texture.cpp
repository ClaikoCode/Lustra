#include "Texture.h"

#include "Graphics.h"
#include "GraphicsUtils.h"
#include "LustraLib/Assert.h"

using namespace detail;

namespace
{
	// Returns the image aspect related to a given format.
	// Checks all formats that are essentially set in stone and assumes any other remaining format is a color format.
	constexpr vk::ImageAspectFlags AspectOf(vk::Format format)
	{
		using enum vk::ImageAspectFlagBits;
		switch (format)
		{
			// --- Depth ---
			case vk::Format::eD16Unorm:
			case vk::Format::eD32Sfloat:
				return eDepth;

			case vk::Format::eD16UnormS8Uint:
			case vk::Format::eD24UnormS8Uint:
			case vk::Format::eD32SfloatS8Uint:
				return eDepth | eStencil;

			// --- Stencil-only ---
			case vk::Format::eS8Uint:
				return eStencil;

			// --- Multi-planar YCbCr ---
			case vk::Format::eG8B8R83Plane420Unorm:
			case vk::Format::eG8B8R83Plane422Unorm:
			case vk::Format::eG8B8R83Plane444Unorm:
				return ePlane0 | ePlane1 | ePlane2;

			case vk::Format::eG8B8R82Plane420Unorm:
			case vk::Format::eG8B8R82Plane422Unorm:
				return ePlane0 | ePlane1;

			// --- Undefined case ---
			case vk::Format::eUndefined:
				return eNone;

			// --- Color (only ones left) ---
			default:
				return eColor;
		}
	}

	[[nodiscard]] vk::ResultValue<ImageAllocation> AllocateImage(const vk::ImageCreateInfo& imageInfo)
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
		vmaDestroyImage(Graphics::gVmaAllocator, imageAllocation.image, imageAllocation.vmaAllocation);
		imageAllocation = {}; // Reset handles.
	}

	[[nodiscard]] vk::ResultValue<ImageAllocation> CreateTexture2D(const Resource::TextureDesc2D& texDesc)
	{
		const vk::ImageCreateInfo depthCreateInfo = {
		    .imageType     = vk::ImageType::e2D,
		    .format        = texDesc.format,
		    .extent        = {.width = texDesc.width, .height = texDesc.height, .depth = 1},
		    .mipLevels     = 1,
		    .arrayLayers   = 1,
		    .samples       = vk::SampleCountFlagBits::e1,
		    .tiling        = vk::ImageTiling::eOptimal,
		    .usage         = texDesc.usage,
		    .initialLayout = vk::ImageLayout::eUndefined
		};

		return AllocateImage(depthCreateInfo);
	}
} // namespace

namespace Resource
{
	void CreateDepthTexture(Handle<DepthTexture> depthTex, const TextureDesc2D& depthDesc)
	{
		ENSURE(depthTex.Get() != nullptr);

		vk::ImageAspectFlags depthAspect = AspectOf(depthDesc.format);
		ENSURE_EX(
		    static_cast<bool>(depthAspect & vk::ImageAspectFlagBits::eDepth),
		    "Could not get valid depth aspect from format. Check that format is valid."
		);

		DepthTexture& depthTexture = *depthTex.Get();
		depthTexture.desc          = depthDesc;

		depthTexture.allocation = AssertVk(CreateTexture2D(depthDesc));

		const vk::ImageViewCreateInfo depthViewInfo = {
		    .image            = depthTexture.allocation.image,
		    .viewType         = vk::ImageViewType::e2D,
		    .format           = depthDesc.format,
		    .subresourceRange = {.aspectMask = depthAspect, .levelCount = 1, .layerCount = 1}
		};

		depthTexture.view =
		    AssertVk(Graphics::gVkDevice.createImageView(depthViewInfo, Graphics::gAllocationCallbacks));
	} // namespace Resource

	void DestroyDepthTexture(Handle<DepthTexture> depthTex)
	{
		DepthTexture* depthTexturePtr = depthTex.Get();
		ENSURE(depthTexturePtr != nullptr);

		if (depthTexturePtr->view)
		{
			Graphics::gVkDevice.destroyImageView(depthTexturePtr->view, Graphics::gAllocationCallbacks);
		}

		if (depthTexturePtr->allocation.image)
		{
			FreeImageAllocation(depthTexturePtr->allocation);
		}
	}

	void ResizeDepthTexture(Handle<DepthTexture> depthTex, uint32_t newWidth, uint32_t newHeight)
	{
		DepthTexture* depthTexPtr = depthTex.Get();
		ENSURE(depthTexPtr != nullptr);

		// Destroy the resources at the handle.
		Resource::DestroyDepthTexture(depthTex);

		// Use its own description to fill the new dimenions and create it once again.
		TextureDesc2D newDesc = depthTexPtr->desc;
		newDesc.width         = newWidth;
		newDesc.height        = newHeight;

		Resource::CreateDepthTexture(depthTex, newDesc);
	}
} // namespace Resource
