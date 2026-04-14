#pragma once

#include "vulkan/vulkan.h"

#include <cstdint>
#include <string_view>
#include <vector>

#if defined(_DEBUG) || defined(_LUSTRA_FORCE_VULKAN_DEBUG_LAYERS)
	#define USE_VALIDATION_LAYERS (true)
#else
	#define USE_VALIDATION_LAYERS (false)
#endif

namespace Graphics
{
	constexpr uint32_t gEngineVersion       = VK_MAKE_API_VERSION(0, 1, 0, 0);
	constexpr uint32_t gApplicationVersion  = VK_MAKE_API_VERSION(0, 1, 0, 0);
	constexpr uint32_t gTargetVulkanVersion = VK_API_VERSION_1_3;
	constexpr bool gUseValidationLayers     = USE_VALIDATION_LAYERS;
	// Nullptr placeholder that might be used in the future.
	constexpr VkAllocationCallbacks* gVkAllocationCallbacks = nullptr;

	inline VkInstance gVkInstance                     = {};
	inline VkPhysicalDevice gVkPhysicalDevice         = {};
	inline VkDevice gVkDevice                         = {};
	inline VkDebugUtilsMessengerEXT gVkDebugMessenger = nullptr;

	// Creates the Vulkan instance and Vulkan device with several checks on extensions and layers.
	// Can be supplied with external
	void SetupVulkan(const std::string_view appName, const std::vector<const char*>& externalRequestedExtensions);
	void TearDownVulkan();

	void SetupInstance(
	    const VkApplicationInfo& vkApplicationInfo, const std::vector<const char*>& externalRequestedExtensions
	);
	void SetupDevice();
	void SetupDebugMessenger();
} // namespace Graphics
