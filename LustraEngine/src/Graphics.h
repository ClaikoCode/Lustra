#pragma once

#include "LustraVulkan.h"
#include "Window.h"
#include "vma/vk_mem_alloc.h"

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
	constexpr uint32_t gEngineVersion       = vk::makeApiVersion(0, 0, 1, 0);
	constexpr uint32_t gApplicationVersion  = vk::makeApiVersion(0, 0, 1, 0);
	constexpr uint32_t gTargetVulkanVersion = vk::ApiVersion14;
	constexpr bool gUseValidationLayers     = USE_VALIDATION_LAYERS;
	// Nullptr placeholder that might be used in the future.
	constexpr vk::AllocationCallbacks* gAllocationCallbacks = nullptr;

	inline vk::Instance gVkInstance                     = VK_NULL_HANDLE;
	inline vk::DebugUtilsMessengerEXT gVkDebugMessenger = VK_NULL_HANDLE;
	inline vk::PhysicalDevice gVkPhysicalDevice         = VK_NULL_HANDLE;
	inline vk::Device gVkDevice                         = VK_NULL_HANDLE;
	inline vk::SurfaceKHR gVkSurface                    = VK_NULL_HANDLE;
	inline vk::SwapchainKHR gVkSwapchain                = VK_NULL_HANDLE;

	constexpr vk::SurfaceFormatKHR gTargetSurfaceFormat = {
	    .format = vk::Format::eR8G8B8A8Srgb, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear
	};

	inline std::vector<vk::Image> gSwapchainImages = {};

	struct CommandQueue
	{
		vk::Queue queue = {};
		uint32_t index  = UINT32_MAX; // Queue index
	};

	// Graphics, compute, and transfer capabilities.
	inline CommandQueue graphicsQueue = {};
	// Compute and transfer capabilities.
	inline CommandQueue computeQueue = {};
	// Only transfer capabilities.
	inline CommandQueue transferQueue = {};

	// VMA
	inline VmaAllocator gVmaAllocator = {};

	// Creates the Vulkan instance and Vulkan device with several checks on extensions and layers.
	void SetupVulkan(std::string_view appName, const Window& window);
	void TearDownVulkan();

	void SetupInstance(
	    const vk::ApplicationInfo& vkApplicationInfo, const std::vector<const char*>& externalRequestedExtensions
	);
	void SetupDevice();
	void SetupDebugMessenger();
	void SetupSurfaceAndSwapchain(const Window& window);
	void SetupVMA();

	void Render();
} // namespace Graphics
