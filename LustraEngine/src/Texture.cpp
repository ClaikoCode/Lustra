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

	uint32_t ResolveMipCount(const Resource::TextureDesc2D& texDesc)
	{
		return texDesc.mipLevels == 0
		           ? static_cast<uint32_t>(std::floor(std::log2(std::max(texDesc.width, texDesc.height)))) + 1u
		           : texDesc.mipLevels;
	}

	// Puts a pipeline barrier given the arguments. Also updates the texture's layout.
	void RecordImageBarrier(
	    vk::CommandBuffer cmd,
	    Resource::Texture2D& tex,
	    vk::ImageLayout newLayout,
	    vk::AccessFlags2 srcAccess,
	    vk::AccessFlags2 dstAccess,
	    vk::PipelineStageFlags2 srcStage,
	    vk::PipelineStageFlags2 dstStage
	)
	{
		vk::ImageMemoryBarrier2 imageBarrier = {
		    .srcStageMask        = srcStage,
		    .srcAccessMask       = srcAccess,
		    .dstStageMask        = dstStage,
		    .dstAccessMask       = dstAccess,
		    .oldLayout           = tex.layout,
		    .newLayout           = newLayout,
		    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		    .image               = tex,
		    .subresourceRange    = {
		        .aspectMask     = ::AspectOf(tex.desc.format),
		        .baseMipLevel   = 0,
		        .levelCount     = vk::RemainingMipLevels,
		        .baseArrayLayer = 0,
		        .layerCount     = 1
		    }
		};

		vk::DependencyInfo depInfo = {};
		depInfo.setImageMemoryBarriers(imageBarrier);

		cmd.pipelineBarrier2(depInfo);

		tex.layout = newLayout;
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
		const vk::ImageCreateInfo imageCreateInfo = {
		    .imageType     = vk::ImageType::e2D,
		    .format        = texDesc.format,
		    .extent        = {.width = texDesc.width, .height = texDesc.height, .depth = 1},
		    .mipLevels     = ::ResolveMipCount(texDesc),
		    .arrayLayers   = 1,
		    .samples       = vk::SampleCountFlagBits::e1,
		    .tiling        = vk::ImageTiling::eOptimal, // Optimal for GPU reading (NOT CPU READABLE)
		    .usage         = texDesc.usage,
		    .sharingMode   = vk::SharingMode::eExclusive,
		    .initialLayout = vk::ImageLayout::eUndefined,
		};

		return AllocateImage(imageCreateInfo);
	}

	// Will leave the image layout in TransferDstOptimal.
	void RecordCopyBufferToImage(
	    vk::CommandBuffer cmd, Resource::Texture2D& dst, AllocatedBuffer& src, vk::Extent2D extent
	)
	{
		::RecordImageBarrier(
		    cmd,
		    dst,
		    vk::ImageLayout::eTransferDstOptimal,
		    vk::AccessFlagBits2::eNone,
		    vk::AccessFlagBits2::eTransferWrite,
		    vk::PipelineStageFlagBits2::eNone,
		    vk::PipelineStageFlagBits2::eCopy
		);

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
		cmd.copyBufferToImage(src.buffer, dst, vk::ImageLayout::eTransferDstOptimal, region);
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
		    .subresourceRange = {
		        .aspectMask   = imageAspect,
		        .baseMipLevel = 0,
		        .levelCount   = vk::RemainingMipLevels,
		        .layerCount   = 1,
		    }
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
		texDesc.usage |= vk::ImageUsageFlagBits::eTransferDst;

		const uint32_t requestedMipCount = ::ResolveMipCount(texDesc);
		const bool shouldCreateMips      = requestedMipCount > 1;

		// If we are building mip maps, also add the transfer src bit.
		if (shouldCreateMips)
		{
			texDesc.usage |= vk::ImageUsageFlagBits::eTransferSrc;
		}

		CreateTexture2D(name, textureHandle, texDesc);
		Texture2D& texture = GetRef(textureHandle);

		AllocatedBuffer uploadBuffer = CreateUploadBuffer(imageData.data(), imageData.size_bytes());

		vk::CommandBuffer cmd = Graphics::BeginSingleTimeCommands();

		// Upload data to created texture.
		RecordCopyBufferToImage(
		    cmd,
		    texture,
		    uploadBuffer,
		    vk::Extent2D{
		        .width  = texture.desc.width,
		        .height = texture.desc.height,
		    }
		);

		if (shouldCreateMips)
		{
			vk::Image textureImage = texture.allocation.image;

			// These values will be the same for all barriers between mips.
			vk::ImageMemoryBarrier2 imageBarrier         = {};
			imageBarrier.srcStageMask                    = vk::PipelineStageFlagBits2::eAllTransfer;
			imageBarrier.image                           = textureImage;
			imageBarrier.srcQueueFamilyIndex             = vk::QueueFamilyIgnored;
			imageBarrier.dstQueueFamilyIndex             = vk::QueueFamilyIgnored;
			imageBarrier.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
			imageBarrier.subresourceRange.baseArrayLayer = 0;
			imageBarrier.subresourceRange.layerCount     = 1;
			imageBarrier.subresourceRange.levelCount     = 1;

			vk::DependencyInfo depInfo = {};
			depInfo.setImageMemoryBarriers(imageBarrier);

			uint32_t srcWidth  = texDesc.width;
			uint32_t srcHeight = texDesc.height;

			vk::ImageLayout oldLayout = texture.layout;
			for (uint32_t mipIndex = 0; mipIndex < requestedMipCount - 1; mipIndex++)
			{
				uint32_t dstWidth  = std::max(1u, srcWidth / 2);
				uint32_t dstHeight = std::max(1u, srcHeight / 2);

				// Prepare mip slice to be a transfer source.
				imageBarrier.subresourceRange.baseMipLevel = mipIndex;
				imageBarrier.dstStageMask                  = vk::PipelineStageFlagBits2::eAllTransfer;
				imageBarrier.oldLayout                     = oldLayout;
				imageBarrier.newLayout                     = vk::ImageLayout::eTransferSrcOptimal;
				imageBarrier.srcAccessMask                 = vk::AccessFlagBits2::eTransferWrite;
				imageBarrier.dstAccessMask                 = vk::AccessFlagBits2::eTransferRead;

				cmd.pipelineBarrier2(depInfo);

				vk::ImageBlit2 blit = {};

				blit.srcOffsets[0] = {.x = 0, .y = 0, .z = 0};
				blit.srcOffsets[1] = {
				    .x = static_cast<int32_t>(srcWidth), .y = static_cast<int32_t>(srcHeight), .z = 1
				};
				blit.srcSubresource = {
				    .aspectMask     = vk::ImageAspectFlagBits::eColor,
				    .mipLevel       = mipIndex,
				    .baseArrayLayer = 0,
				    .layerCount     = 1,
				};

				blit.dstOffsets[0] = {.x = 0, .y = 0, .z = 0};
				blit.dstOffsets[1] = {
				    .x = static_cast<int32_t>(dstWidth), .y = static_cast<int32_t>(dstHeight), .z = 1
				};
				blit.dstSubresource = {
				    .aspectMask     = vk::ImageAspectFlagBits::eColor,
				    .mipLevel       = mipIndex + 1,
				    .baseArrayLayer = 0,
				    .layerCount     = 1,
				};

				vk::BlitImageInfo2 blitInfo = {
				    .srcImage       = textureImage,
				    .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
				    .dstImage       = textureImage,
				    .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
				    .filter         = vk::Filter::eLinear,
				};
				blitInfo.setRegions(blit);

				cmd.blitImage2(blitInfo);

				// Transition mip slice into being read only.
				imageBarrier.dstStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
				imageBarrier.oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
				imageBarrier.newLayout     = vk::ImageLayout::eReadOnlyOptimal;
				imageBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead;
				imageBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;

				cmd.pipelineBarrier2(depInfo);

				// Next dims is the same as the mip just written to.
				srcWidth  = dstWidth;
				srcHeight = dstHeight;
			}

			// Last mip slice is never transitioned in the loop, so its done here.
			imageBarrier.subresourceRange.baseMipLevel = requestedMipCount - 1;
			imageBarrier.oldLayout                     = oldLayout;
			imageBarrier.newLayout                     = vk::ImageLayout::eReadOnlyOptimal;
			imageBarrier.dstStageMask                  = vk::PipelineStageFlagBits2::eAllCommands;
			imageBarrier.srcAccessMask                 = vk::AccessFlagBits2::eTransferWrite;
			imageBarrier.dstAccessMask                 = vk::AccessFlagBits2::eShaderRead;

			cmd.pipelineBarrier2(depInfo);

			// Make sure to match texture layout with layout of all mip maps.
			texture.layout = vk::ImageLayout::eReadOnlyOptimal;
		}
		else
		{
			::RecordImageBarrier(
			    cmd,
			    texture,
			    vk::ImageLayout::eReadOnlyOptimal,
			    vk::AccessFlagBits2::eTransferWrite,
			    vk::AccessFlagBits2::eShaderRead,
			    vk::PipelineStageFlagBits2::eTransfer,
			    vk::PipelineStageFlagBits2::eFragmentShader
			);
		}

		Graphics::EndSingleTimeCommands(cmd);

		DestroyBuffer(uploadBuffer);
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
