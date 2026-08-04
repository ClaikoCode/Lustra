#pragma once

#include "LustraVulkan.h"

struct AllocatedBuffer
{
	vk::Buffer buffer;
	VmaAllocation alloc;
	VmaAllocationInfo info;
};

// Create a buffer through VMA with AUTO memory usage.
[[nodiscard]] AllocatedBuffer CreateBuffer(
    size_t sizeInBytes, vk::BufferUsageFlags usageFlags, VmaAllocationCreateFlags allocFlags
);

// Calls vmaDestroyBuffer() and resets member fields.
void DestroyBuffer(AllocatedBuffer& buffer);

// Copies data from src to dst and waits on a fence till copy is done.
void CopyBuffers(AllocatedBuffer& dst, AllocatedBuffer& src, size_t sizeInBytes);

// Creates an upload buffer that will contain the data pointed to by uploadData.
[[nodiscard]] AllocatedBuffer CreateUploadBuffer(const void* uploadData, size_t sizeInBytes);

// Creates a host-visible buffer and uploads the CPU data through a copy transfer.
[[nodiscard]] AllocatedBuffer CreateBufferFromCPUData(
    void* uploadData, size_t sizeInBytes, vk::BufferUsageFlags usageFlags, VmaAllocationCreateFlags allocFlags
);

// Will check the memory type of dest buffer and choose the appropriate method of uploading the data.
// Host visible and device local: 	Memcpy through BAR / ReBAR.
// Device local only: 				Temporary staging buffer with copy command.
// Host visible only: 				Regular memcpy. Should not be used if the GPU uses the memory frequently.
void UploadData(const void* data, size_t sizeInBytes, AllocatedBuffer& dst);
