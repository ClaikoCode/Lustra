#pragma once

#include "vulkan/vulkan.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Graphics
{
	constexpr uint32_t gEngineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	constexpr uint32_t gApplicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	constexpr uint32_t gTargetVulkanVersion = VK_API_VERSION_1_3;

	inline VkInstance gVkInstance = {};
	inline VkPhysicalDevice gVkPhysicalDevice = {};
	inline VkDevice gVkDevice = {};

	// Creates the Vulkan instance and Vulkan device with several checks on extensions and layers.
	// Can be supplied with external
	void SetupVulkan(const std::string_view appName, const std::vector<const char*>& externalRequestedExtensions);
	void TearDownVulkan();
} // namespace Graphics
