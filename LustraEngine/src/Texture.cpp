#include "Texture.h"

#include "Buffer.h"
#include "Graphics.h"
#include "GraphicsUtils.h"
#include "LustraLib/Assert.h"
#include "Resource.h"

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

	[[nodiscard]] vk::ResultValue<ImageAllocation> AllocateTexture2D(const Resource::TextureDesc2D& texDesc)
	{
		const vk::ImageCreateInfo depthCreateInfo = {
		    .imageType     = vk::ImageType::e2D,
		    .format        = texDesc.format,
		    .extent        = {.width = texDesc.width, .height = texDesc.height, .depth = 1},
		    .mipLevels     = texDesc.mipLevels == 0 ? 1 : texDesc.mipLevels, // TODO: Check for 0 and put max.
		    .arrayLayers   = 1,
		    .samples       = vk::SampleCountFlagBits::e1,
		    .tiling        = vk::ImageTiling::eOptimal, // Optimal for GPU reading (NOT CPU READABLE)
		    .usage         = texDesc.usage,
		    .sharingMode   = vk::SharingMode::eExclusive,
		    .initialLayout = vk::ImageLayout::eUndefined,
		};

		return AllocateImage(depthCreateInfo);
	}

	void CopyBufferToImage(ImageAllocation& dst, AllocatedBuffer& src, vk::Extent2D extent)
	{
		vk::FenceCreateInfo fenceInfo = {};
		vk::Fence fence = AssertVk(Graphics::gVkDevice.createFence(fenceInfo, Graphics::gAllocationCallbacks));

		vk::CommandBufferAllocateInfo cmdAllocInfo = {
		    .commandPool        = Graphics::gTransferPool,
		    .commandBufferCount = 1,
		};

		vk::CommandBuffer cmd = AssertVk(Graphics::gVkDevice.allocateCommandBuffers(cmdAllocInfo))[0];

		vk::CommandBufferBeginInfo cmdBeginInfo = {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
		AssertVk(cmd.begin(cmdBeginInfo));

		const vk::ImageSubresourceRange range = {
		    .aspectMask     = vk::ImageAspectFlagBits::eColor,
		    .baseMipLevel   = 0,
		    .levelCount     = 1,
		    .baseArrayLayer = 0,
		    .layerCount     = 1,
		};

		const vk::ImageMemoryBarrier2 toDst = {
		    .srcStageMask        = vk::PipelineStageFlagBits2::eNone,
		    .srcAccessMask       = vk::AccessFlagBits2::eNone,
		    .dstStageMask        = vk::PipelineStageFlagBits2::eCopy,
		    .dstAccessMask       = vk::AccessFlagBits2::eTransferWrite,
		    .oldLayout           = vk::ImageLayout::eUndefined,
		    .newLayout           = vk::ImageLayout::eTransferDstOptimal,
		    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		    .image               = dst.image,
		    .subresourceRange    = range,
		};
		cmd.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(toDst));

		// The copy.
		const vk::BufferImageCopy region = {
		    .bufferOffset      = 0,
		    .bufferRowLength   = 0, // 0 = tightly packed
		    .bufferImageHeight = 0,
		    .imageSubresource =
		        {
		            .aspectMask     = vk::ImageAspectFlagBits::eColor,
		            .mipLevel       = 0,
		            .baseArrayLayer = 0,
		            .layerCount     = 1,
		        },
		    .imageOffset = {.x = 0, .y = 0, .z = 0},
		    .imageExtent = {
		        .width  = extent.width,
		        .height = extent.height,
		        .depth  = 1,
		    },
		};
		cmd.copyBufferToImage(src.buffer, dst.image, vk::ImageLayout::eTransferDstOptimal, region);

		const vk::ImageMemoryBarrier2 toRead = {
		    .srcStageMask        = vk::PipelineStageFlagBits2::eCopy,
		    .srcAccessMask       = vk::AccessFlagBits2::eTransferWrite,
		    .dstStageMask        = vk::PipelineStageFlagBits2::eFragmentShader,
		    .dstAccessMask       = vk::AccessFlagBits2::eShaderRead,
		    .oldLayout           = vk::ImageLayout::eTransferDstOptimal,
		    .newLayout           = vk::ImageLayout::eShaderReadOnlyOptimal,
		    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		    .image               = dst.image,
		    .subresourceRange    = range,
		};
		cmd.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(toRead));

		AssertVk(cmd.end());

		vk::SubmitInfo submitInfo = {};
		submitInfo.setCommandBuffers(cmd);

		AssertVk(Graphics::GetQueueUsedForTransfers().queue.submit(submitInfo, fence));

		vk::Result result = Graphics::gVkDevice.waitForFences(fence, vk::True, GraphicsUtils::TimeoutTimeS(5));

		if (result == vk::Result::eTimeout)
		{
			PRINT_ERROR("Timed out trying to copy buffer to image.");
		}

		Graphics::gVkDevice.freeCommandBuffers(Graphics::gTransferPool, cmd);
		Graphics::gVkDevice.destroyFence(fence, Graphics::gAllocationCallbacks);
	}
} // namespace

namespace Resource
{

	void CreateTexture2D(Handle<Texture2D> textureHandle, const TextureDesc2D& texDesc)
	{
		ENSURE(Get(textureHandle) != nullptr);

		Texture2D& texture2D = GetRef(textureHandle);

		texture2D.allocation = AssertVk(AllocateTexture2D(texDesc));
		texture2D.desc       = texDesc;

		vk::ImageAspectFlags imageAspect       = AspectOf(texDesc.format);
		const vk::ImageViewCreateInfo viewInfo = {
		    .image            = texture2D.allocation.image,
		    .viewType         = vk::ImageViewType::e2D,
		    .format           = texDesc.format,
		    .subresourceRange = {.aspectMask = imageAspect, .levelCount = 1, .layerCount = 1}
		};

		texture2D.view = AssertVk(Graphics::gVkDevice.createImageView(viewInfo, Graphics::gAllocationCallbacks));
	}

	void CreateReadOnlyTexture2D(
	    Handle<Texture2D> textureHandle, TextureDesc2D& texDesc, std::span<const std::byte> imageData
	)
	{
		// This texture is going to be copied to.
		texDesc.usage = texDesc.usage | vk::ImageUsageFlagBits::eTransferDst;

		CreateTexture2D(textureHandle, texDesc);

		// Upload data to created texture.
		{
			AllocatedBuffer uploadBuffer = CreateUploadBuffer(imageData.data(), imageData.size_bytes());

			Texture2D& texture = GetRef(textureHandle);

			CopyBufferToImage(
			    texture.allocation,
			    uploadBuffer,
			    vk::Extent2D{
			        .width  = texture.desc.width,
			        .height = texture.desc.height,
			    }
			);

			DestroyBuffer(uploadBuffer);
		}
	}

	// TODO: De-duplicate this code by merging depth with texture2d.
	void DestroyTexture2D(Handle<Texture2D> tex)
	{
		Texture2D* texPtr = Get(tex);
		ENSURE(texPtr != nullptr);

		if (texPtr->view)
		{
			Graphics::gVkDevice.destroyImageView(texPtr->view, Graphics::gAllocationCallbacks);
		}

		if (texPtr->allocation.image)
		{
			FreeImageAllocation(texPtr->allocation);
		}
	}

	void CreateDepthTexture(Handle<DepthTexture> depthTex, const TextureDesc2D& depthDesc)
	{
		ENSURE(Get(depthTex) != nullptr);

		vk::ImageAspectFlags depthAspect = AspectOf(depthDesc.format);
		ENSURE_EX(
		    static_cast<bool>(depthAspect & vk::ImageAspectFlagBits::eDepth),
		    "Could not get valid depth aspect from format. Check that format is valid."
		);

		DepthTexture& depthTexture = *Get(depthTex);
		depthTexture.desc          = depthDesc;

		depthTexture.allocation = AssertVk(AllocateTexture2D(depthDesc));

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
		DepthTexture* depthTexturePtr = Get(depthTex);
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
		DepthTexture* depthTexPtr = Get(depthTex);
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
