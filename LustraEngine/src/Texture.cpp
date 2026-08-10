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

	void CreateTexture2D(std::string_view name, Handle<Texture2D> textureHandle, const TextureDesc2D& texDesc)
	{
		ENSURE(Get(textureHandle) != nullptr);

		Texture2D& texture2D = GetRef(textureHandle);

		if (!name.empty())
		{
			texture2D.name = name;
		}

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

		NameVk(Graphics::gVkDevice, texture2D.allocation.image, texture2D.name);
		NameVk(Graphics::gVkDevice, texture2D.view, texture2D.name + ".View");
		NameVma(Graphics::gVmaAllocator, texture2D.allocation.vmaAllocation, texture2D.name);
	} // namespace Resource

	void CreateReadOnlyTexture2D(
	    std::string_view name,
	    Handle<Texture2D> textureHandle,
	    TextureDesc2D& texDesc,
	    std::span<const std::byte> imageData
	)
	{
		// This texture is going to be copied to.
		texDesc.usage = texDesc.usage | vk::ImageUsageFlagBits::eTransferDst;

		CreateTexture2D(name, textureHandle, texDesc);

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

	void CreateDepthTexture(std::string_view name, Handle<Texture2D> depthTex, const TextureDesc2D& depthDesc)
	{
		vk::ImageAspectFlags depthAspect = AspectOf(depthDesc.format);
		ENSURE_EX(
		    static_cast<bool>(depthAspect & vk::ImageAspectFlagBits::eDepth),
		    "Could not get valid depth aspect from format. Check that format is valid."
		);

		CreateTexture2D(name, depthTex, depthDesc);
	}

	void ResizeTexture(Handle<Texture2D> tex, uint32_t newWidth, uint32_t newHeight)
	{
		Texture2D* texPtr = Get(tex);
		ENSURE(texPtr != nullptr);

		// Destroy the resources at the handle.
		Resource::DestroyTexture2D(tex);

		// Use its own description to fill the new dimenions and create it once again.
		TextureDesc2D newDesc = texPtr->desc;
		newDesc.width         = newWidth;
		newDesc.height        = newHeight;

		CreateTexture2D(texPtr->name, tex, newDesc);
	}

	[[nodiscard]] Handle<Texture2D> GetMissingTexture()
	{
		static Handle<Texture2D> missingTexHandle = nullhandle;

		if (missingTexHandle == nullhandle)
		{
			const uint32_t defaultTexSize   = 256u;
			const size_t textureSizeInBytes = 4ull * defaultTexSize * defaultTexSize;

			std::vector<std::byte> albedoColors(textureSizeInBytes, std::byte(0u));
			const uint32_t checkerSquareSize = 16u;
			// Checkered magenta and black albedo texture.
			for (uint32_t i = 0; i < albedoColors.size(); i += 4u)
			{
				// Four bytes per pixel
				const uint32_t pixelIndex = i / 4u;

				const uint32_t x = pixelIndex % defaultTexSize;
				const uint32_t y = pixelIndex / defaultTexSize;

				const bool xEvenSquare = (x / checkerSquareSize) % 2 == 0;
				const bool yEvenSquare = (y / checkerSquareSize) % 2 == 0;

				// 00 = black, 10 = magenta, 01, = magenta, 11 = black
				const bool writeMagenta = xEvenSquare ^ yEvenSquare;

				if (writeMagenta)
				{
					// Color in R and B channel = magenta.
					albedoColors[i]     = std::byte(255u);
					albedoColors[i + 2] = std::byte(255u);
				}

				// Alpha
				albedoColors[i + 3] = std::byte(255u);
			}

			Resource::TextureDesc2D texDesc = {
			    .width     = defaultTexSize,
			    .height    = defaultTexSize,
			    .format    = vk::Format::eR8G8B8A8Srgb,
			    .usage     = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			    .mipLevels = 1,
			};

			missingTexHandle = Resource::AllocateNonOwning<Resource::Texture2D>();

			Resource::CreateReadOnlyTexture2D("MissingTexture", missingTexHandle, texDesc, albedoColors);
		}

		return missingTexHandle;
	}
} // namespace Resource
