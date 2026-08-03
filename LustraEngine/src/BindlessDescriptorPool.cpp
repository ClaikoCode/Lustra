#include "BindlessDescriptorPool.h"

#include "Graphics.h"
#include "GraphicsUtils.h"
#include "LustraLib/Assert.h"

constexpr uint32_t kMaxDescriptorCount = 4096u;

void SlotAllocator::Initialize(uint32_t cap)
{
	capacity = cap;

	// Save 0 as fallback slot.
	for (uint32_t i = capacity; i > 0; i--)
	{
		freeSlots.push(i);
	}
}

uint32_t SlotAllocator::Allocate()
{
	// Return fallback slot.
	if (freeSlots.empty())
	{
		return 0;
	}

	uint32_t allocatedSlot = freeSlots.top();
	freeSlots.pop();

	return allocatedSlot;
}

void SlotAllocator::Free(uint32_t slot)
{
	ENSURE(slot < capacity);

	// Consume fallback slot.
	if (slot == 0)
	{
		return;
	}

	freeSlots.push(slot);
}

void BindlessDescriptorPool::Initialize(uint32_t cap)
{
	m_allocator.Initialize(cap);

	// Create layout
	{
		const vk::DescriptorSetLayoutBinding bindingLayout = {
		    .binding         = kTextureBinding,
		    .descriptorType  = vk::DescriptorType::eCombinedImageSampler, // Might separate in the future.
		    .descriptorCount = kMaxDescriptorCount,
		    .stageFlags      = vk::ShaderStageFlagBits::eAll
		};

		// Three flags to mark bindless.
		// Partially bound allows for empty slots to be valid.
		// Variable descriptor count lets the actual allocation SET AT RUNTIME to be different size smaller than the
		// max. Requires CountAllocateInfo struct to be passed for the pNext field of the allocate info struct.
		const vk::DescriptorBindingFlags flags = vk::DescriptorBindingFlagBits::eUpdateAfterBind |
		                                         vk::DescriptorBindingFlagBits::ePartiallyBound |
		                                         vk::DescriptorBindingFlagBits::eVariableDescriptorCountEXT;

		vk::DescriptorSetLayoutBindingFlagsCreateInfo flagInfo = {};
		flagInfo.setBindingFlags(flags);

		vk::DescriptorSetLayoutCreateInfo descLayoutInfo = {
		    .pNext = &flagInfo,
		    .flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
		};
		descLayoutInfo.setBindings(bindingLayout);

		m_descLayout =
		    AssertVk(Graphics::gVkDevice.createDescriptorSetLayout(descLayoutInfo, Graphics::gAllocationCallbacks));
	}

	// Create pool
	{
		vk::DescriptorPoolSize poolSize = {
		    .type            = vk::DescriptorType::eCombinedImageSampler,
		    .descriptorCount = kMaxDescriptorCount,
		};

		// Only a single set.
		vk::DescriptorPoolCreateInfo poolInfo = {
		    .flags   = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
		    .maxSets = 1,
		};
		poolInfo.setPoolSizes(poolSize);

		m_pool = AssertVk(Graphics::gVkDevice.createDescriptorPool(poolInfo, Graphics::gAllocationCallbacks));
	}

	// Allocate the set with runtime size
	{
		vk::DescriptorSetVariableDescriptorCountAllocateInfo countInfo = {
		    .descriptorSetCount = 1,
		    .pDescriptorCounts  = &cap,
		};

		vk::DescriptorSetAllocateInfo descAllocInfo = {
		    .pNext          = &countInfo,
		    .descriptorPool = m_pool,
		};
		descAllocInfo.setSetLayouts(m_descLayout);

		m_bindlessSet = AssertVk(Graphics::gVkDevice.allocateDescriptorSets(descAllocInfo))[0];
	}
}

uint32_t BindlessDescriptorPool::RegisterTexture(vk::ImageView view, vk::Sampler sampler)
{
	const uint32_t bindlessSlot = m_allocator.Allocate();

	vk::DescriptorImageInfo imageInfo = {
	    .sampler     = sampler,
	    .imageView   = view,
	    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
	};

	vk::WriteDescriptorSet write = {
	    .dstSet          = m_bindlessSet,
	    .dstBinding      = kTextureBinding,
	    .dstArrayElement = bindlessSlot,
	    .descriptorCount = 1,
	    .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
	    .pImageInfo      = &imageInfo,
	};

	Graphics::gVkDevice.updateDescriptorSets(write, nullptr);

	return bindlessSlot;
}

void BindlessDescriptorPool::Destroy()
{
	Graphics::gVkDevice.destroyDescriptorPool(m_pool);
	Graphics::gVkDevice.destroyDescriptorSetLayout(m_descLayout);

	m_pool       = nullptr;
	m_descLayout = nullptr;
}
