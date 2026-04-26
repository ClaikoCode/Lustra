#pragma once

#include "vulkan/vulkan.h"

#include <vector>

#define ENUM_TO_S(enumValue)                                                                                           \
	case enumValue:                                                                                                    \
		return #enumValue;

// Macros for making EXT functions easier.
// Example: auto vkSetDebugUtilsObjectNameEXT = VK_LOAD_INSTANCE_FUNC(vkSetDebugUtilsObjectNameEXT);
#define VK_LOAD_INSTANCE_FUNC(func) reinterpret_cast<PFN_##func>(vkGetInstanceProcAddr(Graphics::gVkInstance, #func))
#define VK_LOAD_DEVICE_FUNC(func) reinterpret_cast<PFN_##func>(vkGetDeviceProcAddr(Graphics::gVkDevice, #func))

#define ASSERT_VK(vkResult)                                                                                            \
	do                                                                                                                 \
	{                                                                                                                  \
		const VkResult err = (vkResult);                                                                               \
		if (err < 0)                                                                                                   \
		{                                                                                                              \
			PRINT_ERROR("Detected Vulkan Error: {}", Graphics::VkResultToString(err));                                 \
			LUSTRA_ASSERT(false);                                                                                      \
		}                                                                                                              \
	} while (false)

// =================================
// 	 Vulkan Info Struct Templating
// =================================

template <typename T>
struct VkStructType;

#define VK_DEFINE_STYPE(VkType, sTypeValue)                                                                            \
	template <>                                                                                                        \
	struct VkStructType<VkType>                                                                                        \
	{                                                                                                                  \
		static constexpr VkStructureType value = sTypeValue;                                                           \
	}

// clang-format off
VK_DEFINE_STYPE(VkApplicationInfo,					   	VK_STRUCTURE_TYPE_APPLICATION_INFO);
VK_DEFINE_STYPE(VkInstanceCreateInfo,					VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
VK_DEFINE_STYPE(VkDeviceCreateInfo,                    	VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
VK_DEFINE_STYPE(VkImageCreateInfo,                     	VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
VK_DEFINE_STYPE(VkBufferCreateInfo,                    	VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
VK_DEFINE_STYPE(VkMemoryAllocateInfo,                  	VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
VK_DEFINE_STYPE(VkCommandPoolCreateInfo,               	VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
VK_DEFINE_STYPE(VkCommandBufferAllocateInfo,           	VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
VK_DEFINE_STYPE(VkCommandBufferBeginInfo,              	VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
VK_DEFINE_STYPE(VkRenderingInfo,                       	VK_STRUCTURE_TYPE_RENDERING_INFO);
VK_DEFINE_STYPE(VkRenderingAttachmentInfo,             	VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
VK_DEFINE_STYPE(VkPipelineLayoutCreateInfo,            	VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
VK_DEFINE_STYPE(VkGraphicsPipelineCreateInfo,          	VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
VK_DEFINE_STYPE(VkComputePipelineCreateInfo,           	VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
VK_DEFINE_STYPE(VkPipelineShaderStageCreateInfo,       	VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
VK_DEFINE_STYPE(VkShaderModuleCreateInfo,              	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
VK_DEFINE_STYPE(VkDescriptorSetLayoutCreateInfo,       	VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
VK_DEFINE_STYPE(VkDescriptorPoolCreateInfo,            	VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
VK_DEFINE_STYPE(VkDescriptorSetAllocateInfo,           	VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
VK_DEFINE_STYPE(VkWriteDescriptorSet,                  	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
VK_DEFINE_STYPE(VkSubmitInfo2,                         	VK_STRUCTURE_TYPE_SUBMIT_INFO_2);
VK_DEFINE_STYPE(VkPresentInfoKHR,                      	VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
VK_DEFINE_STYPE(VkSwapchainCreateInfoKHR,              	VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
VK_DEFINE_STYPE(VkFenceCreateInfo,                     	VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
VK_DEFINE_STYPE(VkSemaphoreCreateInfo,                 	VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
VK_DEFINE_STYPE(VkImageViewCreateInfo,                 	VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
VK_DEFINE_STYPE(VkSamplerCreateInfo,                   	VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
VK_DEFINE_STYPE(VkFramebufferCreateInfo,               	VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
VK_DEFINE_STYPE(VkDebugUtilsMessengerCreateInfoEXT,		VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
VK_DEFINE_STYPE(VkDebugUtilsObjectNameInfoEXT,         	VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
VK_DEFINE_STYPE(VkDeviceQueueCreateInfo,         		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
VK_DEFINE_STYPE(VkPhysicalDeviceFeatures2,         		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
VK_DEFINE_STYPE(VkQueueFamilyProperties2,         		VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2);
VK_DEFINE_STYPE(VkPhysicalDeviceSurfaceInfo2KHR,        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR);
VK_DEFINE_STYPE(VkSurfaceFormat2KHR,        			VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR);
// clang-format on

struct VkInitProxy
{
	template <typename T>
	constexpr operator T() const
	{
		T s{};
		s.sType = VkStructType<T>::value;
		return s;
	}
};

// Will use template argument deduction and implicit converstion operators to instantiate the correct structure
// pre-filled with the structure type (sType) that is associated with said structure.
// Example: VkInstanceCreateInfo instanceCreateInfo = vkInitStruct();
constexpr VkInitProxy vkInitStruct()
{
	return {};
}

struct VkInitVectorProxy
{
	uint32_t count;

	template <typename T>
	constexpr operator std::vector<T>() const
	{
		std::vector<T> structVector(count);

		for (T& vkStruct : structVector)
		{
			vkStruct = vkInitStruct();
		}

		return structVector;
	}
};

// Similar to vkInitStruct but instead pre fills a vector of vk structs with their correct structure type.
// Type has to be filled even if the next call will fill the struct data. This is because of pNext chaining, which seems
// more prevalent with revisions/updates/extensions of Vulkan getter functions.
// Example: std::vector<VkSurfaceFormat2KHR> surfaceFormats = vkInitStructs(surfaceCount);
inline VkInitVectorProxy vkInitStructs(uint32_t count)
{
	return {count};
}

// =================================
// 	   Graphics Helper Functions
// =================================

namespace Graphics
{
	constexpr const char* VkResultToString(VkResult vkResult)
	{
		switch (vkResult)
		{
			ENUM_TO_S(VK_SUCCESS)
			ENUM_TO_S(VK_NOT_READY)
			ENUM_TO_S(VK_TIMEOUT)
			ENUM_TO_S(VK_EVENT_SET)
			ENUM_TO_S(VK_EVENT_RESET)
			ENUM_TO_S(VK_INCOMPLETE)
			ENUM_TO_S(VK_ERROR_OUT_OF_HOST_MEMORY)
			ENUM_TO_S(VK_ERROR_OUT_OF_DEVICE_MEMORY)
			ENUM_TO_S(VK_ERROR_INITIALIZATION_FAILED)
			ENUM_TO_S(VK_ERROR_DEVICE_LOST)
			ENUM_TO_S(VK_ERROR_MEMORY_MAP_FAILED)
			ENUM_TO_S(VK_ERROR_LAYER_NOT_PRESENT)
			ENUM_TO_S(VK_ERROR_EXTENSION_NOT_PRESENT)
			ENUM_TO_S(VK_ERROR_FEATURE_NOT_PRESENT)
			ENUM_TO_S(VK_ERROR_INCOMPATIBLE_DRIVER)
			ENUM_TO_S(VK_ERROR_FORMAT_NOT_SUPPORTED)
			ENUM_TO_S(VK_ERROR_FRAGMENTED_POOL)
			ENUM_TO_S(VK_ERROR_UNKNOWN)
			default:
				return "UNHANDLED VK RESULT STRING";
		}
	}

	constexpr const char* VkDeviceFeatureToString(size_t fieldIndex)
	{
		constexpr const char* kDeviceFeatureFriendlyNames[] = {
		    "Robust Buffer Access",
		    "Full Draw Index UInt32",
		    "Image Cube Array",
		    "Independent Blend",
		    "Geometry Shader",
		    "Tessellation Shader",
		    "Sample Rate Shading",
		    "Dual Source Blend",
		    "Logic Op",
		    "Multi Draw Indirect",
		    "Draw Indirect First Instance",
		    "Depth Clamp",
		    "Depth Bias Clamp",
		    "Fill Mode Non-Solid",
		    "Depth Bounds",
		    "Wide Lines",
		    "Large Points",
		    "Alpha To One",
		    "Multi Viewport",
		    "Sampler Anisotropy",
		    "Texture Compression ETC2",
		    "Texture Compression ASTC LDR",
		    "Texture Compression BC",
		    "Occlusion Query Precise",
		    "Pipeline Statistics Query",
		    "Vertex Pipeline Stores And Atomics",
		    "Fragment Stores And Atomics",
		    "Shader Tessellation And Geometry Point Size",
		    "Shader Image Gather Extended",
		    "Shader Storage Image Extended Formats",
		    "Shader Storage Image Multisample",
		    "Shader Storage Image Read Without Format",
		    "Shader Storage Image Write Without Format",
		    "Shader Uniform Buffer Array Dynamic Indexing",
		    "Shader Sampled Image Array Dynamic Indexing",
		    "Shader Storage Buffer Array Dynamic Indexing",
		    "Shader Storage Image Array Dynamic Indexing",
		    "Shader Clip Distance",
		    "Shader Cull Distance",
		    "Shader Float64",
		    "Shader Int64",
		    "Shader Int16",
		    "Shader Resource Residency",
		    "Shader Resource Min LOD",
		    "Sparse Binding",
		    "Sparse Residency Buffer",
		    "Sparse Residency Image 2D",
		    "Sparse Residency Image 3D",
		    "Sparse Residency 2 Samples",
		    "Sparse Residency 4 Samples",
		    "Sparse Residency 8 Samples",
		    "Sparse Residency 16 Samples",
		    "Sparse Residency Aliased",
		    "Variable Multisample Rate",
		    "Inherited Queries",
		};

		static_assert(
		    sizeof(kDeviceFeatureFriendlyNames) / sizeof(const char*) ==
		        sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32),
		    "Device feature friendly name array is out of sync with VkPhysicalDeviceFeatures."
		);

		return kDeviceFeatureFriendlyNames[fieldIndex];
	}
} // namespace Graphics
