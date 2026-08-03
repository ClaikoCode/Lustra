#pragma once

#include "LustraVulkan.h"

#include <stack>

constexpr uint32_t kTextureBinding = 0u;

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
	void Initialize(uint32_t cap);

	uint32_t RegisterTexture(vk::ImageView view, vk::Sampler sampler);

	void Destroy();

  private:
	vk::DescriptorPool m_pool;
	vk::DescriptorSet m_bindlessSet;
	vk::DescriptorSetLayout m_descLayout;

	SlotAllocator m_allocator;
};
