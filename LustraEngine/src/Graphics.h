#pragma once

#include "Window.h"
#include "vulkan/vulkan.h"

#include <array>
#include <cstdint>
#include <optional>
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

	inline VkInstance gVkInstance                     = VK_NULL_HANDLE;
	inline VkDebugUtilsMessengerEXT gVkDebugMessenger = VK_NULL_HANDLE;
	inline VkPhysicalDevice gVkPhysicalDevice         = VK_NULL_HANDLE;
	inline VkDevice gVkDevice                         = VK_NULL_HANDLE;
	inline VkSurfaceKHR gVkSurface                    = VK_NULL_HANDLE;
	inline VkSwapchainKHR gVkSwapchain                = VK_NULL_HANDLE;

	constexpr VkSurfaceFormatKHR gTargetSurfaceFormat = {
	    .format = VK_FORMAT_R8G8B8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
	};

	inline std::array<VkImage, 2> gSwapchainImages = {{}};

	inline struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphics;
		std::optional<uint32_t> compute;
		std::optional<uint32_t> transfer;
	} gQueueFamilyIndices;

	// Creates the Vulkan instance and Vulkan device with several checks on extensions and layers.
	void SetupVulkan(std::string_view appName, const Window& window);
	void TearDownVulkan();

	void SetupInstance(
	    const VkApplicationInfo& vkApplicationInfo, const std::vector<const char*>& externalRequestedExtensions
	);
	void SetupDevice();
	void SetupDebugMessenger();
	void SetupSwapchain(const Window& window);
} // namespace Graphics
