#pragma once

#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "LustraVulkan.h"

#include <source_location>
#include <vector>

#define ASSERT_VK(vkResult)                                                                                            \
	do                                                                                                                 \
	{                                                                                                                  \
		const VkResult err = (vkResult);                                                                               \
		if (err < 0)                                                                                                   \
		{                                                                                                              \
			PRINT_ERROR("Detected Vulkan Error: {}", string_VkResult(err));                                            \
			LUSTRA_ASSERT(false);                                                                                      \
		}                                                                                                              \
	} while (false)

template <typename T>
T AssertVk(vk::ResultValue<T> resultValue, std::source_location loc = std::source_location::current())
{
	if (resultValue.result != vk::Result::eSuccess)
	{
		LustraLib::Print(
		    LustraLib::OutputLevelError,
		    loc.file_name(),
		    loc.function_name(),
		    loc.line(),
		    "Detected Vulkan Error: {}",
		    vk::to_string(resultValue.result)
		);

		LUSTRA_ASSERT(false);
	}

	return std::move(resultValue.value);
}

// =================================
// 	 Vulkan Info Struct Templating
// =================================

// template <typename T>
// struct VkStructType;
//
// #define VK_DEFINE_STYPE(VkType, sTypeValue) \
// 	template <>                                                                                                        \
// 	struct VkStructType<VkType>                                                                                        \
// 	{                                                                                                                  \
// 		static constexpr VkStructureType value = sTypeValue;                                                           \
// 	}
//
// // clang-format off
// VK_DEFINE_STYPE(VkApplicationInfo,					   	VK_STRUCTURE_TYPE_APPLICATION_INFO);
// VK_DEFINE_STYPE(VkInstanceCreateInfo,					VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
// VK_DEFINE_STYPE(VkDeviceCreateInfo,                    	VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
// VK_DEFINE_STYPE(VkImageCreateInfo,                     	VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
// VK_DEFINE_STYPE(VkBufferCreateInfo,                    	VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
// VK_DEFINE_STYPE(VkMemoryAllocateInfo,                  	VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
// VK_DEFINE_STYPE(VkCommandPoolCreateInfo,               	VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
// VK_DEFINE_STYPE(VkCommandBufferAllocateInfo,           	VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
// VK_DEFINE_STYPE(VkCommandBufferBeginInfo,              	VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
// VK_DEFINE_STYPE(VkRenderingInfo,                       	VK_STRUCTURE_TYPE_RENDERING_INFO);
// VK_DEFINE_STYPE(VkRenderingAttachmentInfo,             	VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
// VK_DEFINE_STYPE(VkPipelineLayoutCreateInfo,            	VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
// VK_DEFINE_STYPE(VkGraphicsPipelineCreateInfo,          	VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
// VK_DEFINE_STYPE(VkComputePipelineCreateInfo,           	VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
// VK_DEFINE_STYPE(VkPipelineShaderStageCreateInfo,       	VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
// VK_DEFINE_STYPE(VkShaderModuleCreateInfo,              	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
// VK_DEFINE_STYPE(VkDescriptorSetLayoutCreateInfo,       	VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
// VK_DEFINE_STYPE(VkDescriptorPoolCreateInfo,            	VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
// VK_DEFINE_STYPE(VkDescriptorSetAllocateInfo,           	VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
// VK_DEFINE_STYPE(VkWriteDescriptorSet,                  	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
// VK_DEFINE_STYPE(VkSubmitInfo2,                         	VK_STRUCTURE_TYPE_SUBMIT_INFO_2);
// VK_DEFINE_STYPE(VkPresentInfoKHR,                      	VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
// VK_DEFINE_STYPE(VkSwapchainCreateInfoKHR,              	VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
// VK_DEFINE_STYPE(VkFenceCreateInfo,                     	VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
// VK_DEFINE_STYPE(VkSemaphoreCreateInfo,                 	VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
// VK_DEFINE_STYPE(VkImageViewCreateInfo,                 	VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
// VK_DEFINE_STYPE(VkSamplerCreateInfo,                   	VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
// VK_DEFINE_STYPE(VkFramebufferCreateInfo,               	VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
// VK_DEFINE_STYPE(VkDebugUtilsMessengerCreateInfoEXT,		VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
// VK_DEFINE_STYPE(VkDebugUtilsObjectNameInfoEXT,         	VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
// VK_DEFINE_STYPE(VkDeviceQueueCreateInfo,         		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
// VK_DEFINE_STYPE(VkPhysicalDeviceFeatures2,         		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
// VK_DEFINE_STYPE(VkPhysicalDeviceVulkan11Features,       VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
// VK_DEFINE_STYPE(VkPhysicalDeviceVulkan12Features,       VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
// VK_DEFINE_STYPE(VkPhysicalDeviceVulkan13Features,       VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
// VK_DEFINE_STYPE(VkPhysicalDeviceVulkan14Features,       VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES);
// VK_DEFINE_STYPE(VkQueueFamilyProperties2,         		VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2);
// VK_DEFINE_STYPE(VkPhysicalDeviceSurfaceInfo2KHR,        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR);
// VK_DEFINE_STYPE(VkSurfaceFormat2KHR,        			VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR);
// VK_DEFINE_STYPE(VkValidationFeaturesEXT,        		VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT);
// VK_DEFINE_STYPE(VkSurfaceCapabilities2KHR,        		VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR);
// // clang-format on
//
// struct VkInitProxy
// {
// 	template <typename T>
// 	constexpr operator T() const
// 	{
// 		T s{};
// 		s.sType = VkStructType<T>::value;
// 		return s;
// 	}
// };
//
// // Will use template argument deduction and implicit converstion operators to instantiate the correct structure
// // pre-filled with the structure type (sType) that is associated with said structure.
// // Example: VkInstanceCreateInfo instanceCreateInfo = vkInitStruct();
// constexpr VkInitProxy vkInitStruct()
// {
// 	return {};
// }
//
// struct VkInitVectorProxy
// {
// 	uint32_t count;
//
// 	template <typename T>
// 	constexpr operator std::vector<T>() const
// 	{
// 		std::vector<T> structVector(count);
//
// 		for (T& vkStruct : structVector)
// 		{
// 			vkStruct = vkInitStruct();
// 		}
//
// 		return structVector;
// 	}
// };
//
// // Similar to vkInitStruct but instead pre fills a vector of vk structs with their correct structure type.
// // Type has to be filled even if the next call will fill the struct data. This is because of pNext chaining, which
// seems
// // more prevalent with revisions/updates/extensions of Vulkan getter functions.
// // Example: std::vector<VkSurfaceFormat2KHR> surfaceFormats = vkInitStructs(surfaceCount);
// inline VkInitVectorProxy vkInitStructs(uint32_t count)
// {
// 	return {count};
// }
//
// =================================
// 	   Graphics Helper Functions
// =================================

namespace GraphicsUtils
{
	struct FeatureNamesInfo
	{
		const char* const* names;
		uint16_t count;
		uint16_t firstOffset;
	};

	FeatureNamesInfo GetFeatureNames(vk::StructureType structType);

	using FeatureChain = vk::StructureChain<
	    vk::PhysicalDeviceFeatures2,
	    vk::PhysicalDeviceVulkan11Features,
	    vk::PhysicalDeviceVulkan12Features,
	    vk::PhysicalDeviceVulkan13Features,
	    vk::PhysicalDeviceVulkan14Features>;
} // namespace GraphicsUtils
