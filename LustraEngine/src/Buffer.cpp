#include "Buffer.h"

#include "Graphics.h"
#include "GraphicsUtils.h"

AllocatedBuffer CreateBuffer(
    std::string_view name, size_t sizeInBytes, vk::BufferUsageFlags usageFlags, VmaAllocationCreateFlags allocFlags
)
{
	AllocatedBuffer buffer = {};

	const vk::BufferCreateInfo buffInfo = {
	    .size  = sizeInBytes,
	    .usage = usageFlags,
	};

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage                   = VMA_MEMORY_USAGE_AUTO;
	allocInfo.flags                   = allocFlags;

	AssertVk(vmaCreateBuffer(
	    Graphics::gVmaAllocator,
	    buffInfo,
	    &allocInfo,
	    reinterpret_cast<VkBuffer*>(&buffer.buffer),
	    &buffer.alloc,
	    &buffer.info
	));

	// If a request to mapped memory was made, make sure it exists.
	if ((allocFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0u)
	{
		ENSURE(buffer.info.pMappedData != nullptr);
	}

	NameVk(Graphics::gVkDevice, buffer.buffer, name);
	NameVma(Graphics::gVmaAllocator, buffer.alloc, name);
	return buffer;
}

void DestroyBuffer(AllocatedBuffer& buffer)
{
	vmaDestroyBuffer(Graphics::gVmaAllocator, buffer.buffer, buffer.alloc);

	buffer.buffer = VK_NULL_HANDLE;
	buffer.alloc  = {};
}

AllocatedBuffer CreateUploadBuffer(const void* uploadData, size_t sizeInBytes)
{
	const AllocatedBuffer buffer = CreateBuffer(
	    std::format("Upload Buffer ({}B)", sizeInBytes),
	    sizeInBytes,
	    vk::BufferUsageFlagBits::eTransferSrc,
	    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
	);

	memcpy(buffer.info.pMappedData, uploadData, sizeInBytes);
	vmaFlushAllocation(Graphics::gVmaAllocator, buffer.alloc, 0, VK_WHOLE_SIZE);

	return buffer;
}

void CopyBuffers(AllocatedBuffer& dst, AllocatedBuffer& src, size_t sizeInBytes)
{
	const vk::FenceCreateInfo fenceInfo = {};
	vk::Fence uploadFence = AssertVk(Graphics::gVkDevice.createFence(fenceInfo, Graphics::gAllocationCallbacks));

	const vk::CommandBufferAllocateInfo commandAllocInfo = {
	    .commandPool        = Graphics::gTransferPool,
	    .commandBufferCount = 1,
	};

	vk::CommandBuffer cmd = AssertVk(Graphics::gVkDevice.allocateCommandBuffers(commandAllocInfo))[0];

	vk::CommandBufferBeginInfo beginInfo = {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
	AssertVk(cmd.begin(beginInfo));

	cmd.copyBuffer(
	    src.buffer,
	    dst.buffer,
	    vk::BufferCopy{
	        .srcOffset = 0,
	        .dstOffset = 0,
	        .size      = sizeInBytes,
	    }
	);

	AssertVk(cmd.end());

	vk::SubmitInfo submitInfo = {};
	submitInfo.setCommandBuffers(cmd);

	AssertVk(Graphics::GetQueueUsedForTransfers().queue.submit(submitInfo, uploadFence));

	vk::Result result =
	    AssertVk(Graphics::gVkDevice.waitForFences(uploadFence, vk::True, GraphicsUtils::TimeoutTimeS(10)));

	if (result == vk::Result::eTimeout)
	{
		PRINT_ERROR("Buffer copying timed out.");
	}

	// Clean up after copy is done.
	Graphics::gVkDevice.freeCommandBuffers(commandAllocInfo.commandPool, cmd);
	Graphics::gVkDevice.destroyFence(uploadFence);
}

AllocatedBuffer CreateBufferFromCPUData(
    std::string_view name,
    void* uploadData,
    size_t sizeInBytes,
    vk::BufferUsageFlags usageFlags,
    VmaAllocationCreateFlags allocFlags
)
{
	AllocatedBuffer dstBuffer =
	    CreateBuffer(name, sizeInBytes, vk::BufferUsageFlagBits::eTransferDst | usageFlags, allocFlags);

	UploadData(uploadData, sizeInBytes, dstBuffer);

	return dstBuffer;
}

void UploadData(const void* data, size_t sizeInBytes, AllocatedBuffer& dst)
{
	vk::MemoryPropertyFlags memPropertyFlags;
	vmaGetMemoryTypeProperties(
	    Graphics::gVmaAllocator, dst.info.memoryType, reinterpret_cast<VkMemoryPropertyFlags*>(&memPropertyFlags)
	);

	bool isDeviceLocal = static_cast<bool>(memPropertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal);
	bool isHostVisible = static_cast<bool>(memPropertyFlags & vk::MemoryPropertyFlagBits::eHostVisible);

	if (isHostVisible)
	{
		if (!isDeviceLocal)
		{
			PRINT_WARNING(
			    "Uploading data to GPU that is not device local. Every GPU read goes through the PCIe lane. If the GPU "
			    "accesses this data frequently, consider making it local."
			);
		}

		memcpy(dst.info.pMappedData, data, sizeInBytes);
		vmaFlushAllocation(Graphics::gVmaAllocator, dst.alloc, 0, VK_WHOLE_SIZE);
	}
	else if (isDeviceLocal)
	{
		// Upload buffer

		AllocatedBuffer uploadBuffer = CreateUploadBuffer(data, sizeInBytes);

		CopyBuffers(dst, uploadBuffer, sizeInBytes);

		// Destroy temporary upload buffer.
		DestroyBuffer(uploadBuffer);
	}
	else if (isHostVisible)
	{
		// Rare and should hopefully never happen.
	}
	else
	{
		PRINT_ERROR("Cannot upload data to buffer that is neither host visible or device local.");
	}
}
