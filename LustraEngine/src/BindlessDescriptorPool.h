#pragma once

#include "Buffer.h"
#include "LustraVulkan.h"

#include <stack>

enum BindingSlot : uint8_t
{
	BindingSlotMaterialBuffer = 0,
	BindingSlotSamplerTemp,
	BindingSlotTextures,

	BindingSlotCount // Keep as last enum!
};

static_assert(
    BindingSlotTextures == BindingSlotCount - 1,
    "Dynamic count texture bindings has to be last in the binding order. Required by the Vulkan spec."
);

struct SlotAllocator
{
	std::stack<uint32_t> freeSlots;
	uint32_t capacity;

	void Initialize(uint32_t cap);
	uint32_t Allocate();
	void Free(uint32_t slot);
};

class BindlessDescriptorPool
{
  public:
	void Initialize(uint32_t cap, const AllocatedBuffer& matBuffer);

	uint32_t RegisterTexture(vk::ImageView view);

	void Destroy();

	vk::DescriptorSet bindlessSet;
	vk::DescriptorSetLayout descLayout;
	vk::Sampler defaultSampler;

  private:
	vk::DescriptorPool m_pool;

	SlotAllocator m_allocator;
};
