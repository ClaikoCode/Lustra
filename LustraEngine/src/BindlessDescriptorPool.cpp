#include "BindlessDescriptorPool.h"

#include "Graphics.h"
#include "GraphicsUtils.h"
#include "LustraLib/Assert.h"
#include "Resource.h"

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

void BindlessDescriptorPool::Initialize(uint32_t cap, const AllocatedBuffer& matBuffer)
{
	m_allocator.Initialize(cap);
	samplerCache.Initialize();

	// TODO: Delete later.
	{
		Resource::SamplerDesc2D samplerDesc = {
		    .magFilter      = vk::Filter::eLinear,
		    .minFilter      = vk::Filter::eLinear,
		    .addressModeU   = vk::SamplerAddressMode::eRepeat,
		    .addressModeV   = vk::SamplerAddressMode::eRepeat,
		    .usesMipmapMode = false,
		};

		defaultSampler = samplerCache.GetOrCreateSampler2D(samplerDesc);
	}

	// Create layout
	{
		std::array<vk::DescriptorSetLayoutBinding, BindingSlotCount> bindingLayout = {};

		// Material storage buffer. Single, fixed-size.
		bindingLayout[BindingSlotMaterialBuffer] = vk::DescriptorSetLayoutBinding{
		    .binding         = BindingSlotMaterialBuffer,
		    .descriptorType  = vk::DescriptorType::eStorageBuffer,
		    .descriptorCount = 1,
		    .stageFlags      = vk::ShaderStageFlagBits::eFragment,
		};

		bindingLayout[BindingSlotSamplerTemp] = vk::DescriptorSetLayoutBinding{
		    .binding            = BindingSlotSamplerTemp,
		    .descriptorType     = vk::DescriptorType::eSampler,
		    .descriptorCount    = 1,
		    .stageFlags         = vk::ShaderStageFlagBits::eFragment,
		    .pImmutableSamplers = &Resource::GetRef(defaultSampler).sampler,
		};

		// Textures. Variable count. MUST stay the highest binding number.
		bindingLayout[BindingSlotTextures] = vk::DescriptorSetLayoutBinding{
		    .binding         = BindingSlotTextures,
		    .descriptorType  = vk::DescriptorType::eSampledImage,
		    .descriptorCount = kMaxDescriptorCount,
		    .stageFlags      = vk::ShaderStageFlagBits::eFragment,
		};

		// One flag entry per binding, in the same order as bindingLayout.
		// Partially bound allows for empty slots to be valid.
		// Variable descriptor count lets the actual allocation SET AT RUNTIME to be different size smaller than the
		// max. Requires CountAllocateInfo struct to be passed for the pNext field of the allocate info struct.
		std::array<vk::DescriptorBindingFlags, BindingSlotCount> flags = {};
		flags[BindingSlotTextures] =
		    vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eVariableDescriptorCountEXT;

		vk::DescriptorSetLayoutBindingFlagsCreateInfo flagInfo = {};
		flagInfo.setBindingFlags(flags);

		vk::DescriptorSetLayoutCreateInfo descLayoutInfo = {
		    .pNext = &flagInfo,
		};
		descLayoutInfo.setBindings(bindingLayout);

		descLayout =
		    AssertVk(Graphics::gVkDevice.createDescriptorSetLayout(descLayoutInfo, Graphics::gAllocationCallbacks));
	}

	// Create pool
	{
		std::array<vk::DescriptorPoolSize, BindingSlotCount> poolSizes = {};

		poolSizes[BindingSlotMaterialBuffer] = vk::DescriptorPoolSize{
		    .type            = vk::DescriptorType::eStorageBuffer,
		    .descriptorCount = 1,
		};

		poolSizes[BindingSlotSamplerTemp] = vk::DescriptorPoolSize{
		    .type            = vk::DescriptorType::eSampler,
		    .descriptorCount = 1,
		};

		poolSizes[BindingSlotTextures] = vk::DescriptorPoolSize{
		    .type            = vk::DescriptorType::eSampledImage,
		    .descriptorCount = kMaxDescriptorCount,
		};

		vk::DescriptorPoolCreateInfo poolInfo = {
		    .maxSets = 1, // Only a single set.
		};
		poolInfo.setPoolSizes(poolSizes);

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
		descAllocInfo.setSetLayouts(descLayout);

		bindlessSet = AssertVk(Graphics::gVkDevice.allocateDescriptorSets(descAllocInfo))[0];
	}

	// Write the material storage buffer into its binding.
	{
		const vk::DescriptorBufferInfo bufferInfo = {
		    .buffer = matBuffer.buffer,
		    .offset = 0,
		    .range  = VK_WHOLE_SIZE,
		};

		vk::WriteDescriptorSet write = {
		    .dstSet          = bindlessSet,
		    .dstBinding      = BindingSlotMaterialBuffer,
		    .dstArrayElement = 0,
		    .descriptorType  = vk::DescriptorType::eStorageBuffer,
		};
		write.setBufferInfo(bufferInfo); // Sets descriptorCount = 1 and pBufferInfo.

		Graphics::gVkDevice.updateDescriptorSets(write, {});
	}
}

uint32_t BindlessDescriptorPool::RegisterTexture(vk::ImageView view)
{
	const uint32_t bindlessSlot = m_allocator.Allocate();

	vk::DescriptorImageInfo imageInfo = {
	    .imageView   = view,
	    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
	};

	vk::WriteDescriptorSet write = {
	    .dstSet          = bindlessSet,
	    .dstBinding      = BindingSlotTextures,
	    .dstArrayElement = bindlessSlot,
	    .descriptorType  = vk::DescriptorType::eSampledImage,
	};
	write.setImageInfo(imageInfo);

	Graphics::gVkDevice.updateDescriptorSets(write, nullptr);

	PRINT_DEBUG("Registered texture at slot {}", bindlessSlot);

	return bindlessSlot;
}

void BindlessDescriptorPool::Destroy()
{
	Graphics::gVkDevice.destroyDescriptorPool(m_pool);
	Graphics::gVkDevice.destroyDescriptorSetLayout(descLayout);

	samplerCache.Destroy();

	m_pool     = nullptr;
	descLayout = nullptr;
}
