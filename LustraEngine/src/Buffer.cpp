#include "Buffer.h"

#include "Graphics.h"
#include "GraphicsUtils.h"

AllocatedBuffer CreateBuffer(size_t sizeInBytes, vk::BufferUsageFlags usageFlags, VmaAllocationCreateFlags allocFlags)
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

	return buffer;
}

void DestroyBuffer(AllocatedBuffer& buffer)
{
	vmaDestroyBuffer(Graphics::gVmaAllocator, buffer.buffer, buffer.alloc);

	buffer.buffer = VK_NULL_HANDLE;
	buffer.alloc  = {};
}

AllocatedBuffer CreateUploadBuffer(void* uploadData, size_t sizeInBytes)
{
	const AllocatedBuffer buffer = CreateBuffer(
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

	AssertVk(Graphics::transferQueue.queue.submit(submitInfo, uploadFence));

	vk::Result result =
	    AssertVk(Graphics::gVkDevice.waitForFences(uploadFence, 1u, GraphicsUtils::TimeoutTime(10'000)));

	if (result == vk::Result::eTimeout)
	{
		PRINT_ERROR("Buffer copying timed out.");
	}

	// Clean up after copy is done.
	Graphics::gVkDevice.freeCommandBuffers(commandAllocInfo.commandPool, cmd);
	Graphics::gVkDevice.destroyFence(uploadFence);
}

AllocatedBuffer CreateBufferFromCPUData(
    void* uploadData, size_t sizeInBytes, vk::BufferUsageFlags usageFlags, VmaAllocationCreateFlags allocFlags
)
{
	AllocatedBuffer dstBuffer =
	    CreateBuffer(sizeInBytes, vk::BufferUsageFlagBits::eTransferDst | usageFlags, allocFlags);

	AllocatedBuffer uploadBuffer = CreateUploadBuffer(uploadData, sizeInBytes);

	CopyBuffers(dstBuffer, uploadBuffer, sizeInBytes);

	// Destroy temporary upload buffer.
	DestroyBuffer(uploadBuffer);

	return dstBuffer;
}
