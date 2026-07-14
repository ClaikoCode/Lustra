#pragma once

#include "LustraVulkan.h"
#include "Window.h"
#include "vma/vk_mem_alloc.h"

#include <cstdint>
#include <string_view>
#include <vector>

#if defined(_DEBUG) || defined(_LUSTRA_FORCE_VULKAN_DEBUG_LAYERS)
	#define USE_VALIDATION_LAYERS (true)
#else
	#define USE_VALIDATION_LAYERS (false)
#endif

// Graphics related constants and struct definitions.
namespace Graphics
{
	constexpr uint32_t gEngineVersion       = vk::makeApiVersion(0, 0, 1, 0);
	constexpr uint32_t gApplicationVersion  = vk::makeApiVersion(0, 0, 1, 0);
	constexpr uint32_t gTargetVulkanVersion = vk::ApiVersion14;
	constexpr bool gUseValidationLayers     = USE_VALIDATION_LAYERS;

	constexpr vk::SurfaceFormatKHR gTargetSurfaceFormat = {
	    // This format is supported essentially everywhere. Shader color writing to to swapchain is not affected by this
	    // ordering.
	    .format     = vk::Format::eB8G8R8A8Srgb,
	    .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear
	};

	constexpr vk::Format gTargetDepthFormat = vk::Format::eD32Sfloat;

	struct CommandQueue
	{
		vk::Queue queue = {};
		uint32_t index  = UINT32_MAX; // Queue index
	};

	struct Swapchain
	{
		vk::SwapchainKHR swapchain = VK_NULL_HANDLE;
		uint32_t width             = UINT32_MAX;
		uint32_t height            = UINT32_MAX;

		std::vector<vk::Image> images;
		std::vector<vk::ImageView> views;
		std::vector<vk::Semaphore> semaphores;

		void Destroy();

		operator vk::SwapchainKHR() const
		{
			return swapchain;
		}
	};
} // namespace Graphics

namespace Graphics
{
	// Nullptr placeholder that might be used in the future.
	inline vk::Instance gVkInstance                      = VK_NULL_HANDLE;
	inline vk::DebugUtilsMessengerEXT gVkDebugMessenger  = VK_NULL_HANDLE;
	inline vk::PhysicalDevice gVkPhysicalDevice          = VK_NULL_HANDLE;
	inline vk::Device gVkDevice                          = VK_NULL_HANDLE;
	inline vk::SurfaceKHR gVkSurface                     = VK_NULL_HANDLE;
	inline vk::AllocationCallbacks* gAllocationCallbacks = nullptr;

	inline Swapchain gSwapchain;

	// Graphics, compute, and transfer capabilities.
	inline CommandQueue graphicsQueue = {};
	// Compute and transfer capabilities.
	inline CommandQueue computeQueue = {};
	// Only transfer capabilities.
	inline CommandQueue transferQueue = {};

	// VMA
	inline VmaAllocator gVmaAllocator = {};

	// TODO: Find a better way to solve this.
	inline const Window* gWindowPtr = nullptr;

	// Creates the Vulkan instance and Vulkan device with several checks on extensions and layers.
	void SetupVulkan(std::string_view appName, const Window& window);
	void TearDownVulkan();

	void SetupInstance(
	    const vk::ApplicationInfo& vkApplicationInfo, const std::vector<const char*>& externalRequestedExtensions
	);
	void SetupDevice();
	void SetupDebugMessenger();
	void SetupVMA();
	void SetupSurface(const Window& window);
	void CreateSwapchain(const Window& window);

	// Will call vkDeviceWaitIdle() to ensure all graphics work is done beyond the call.
	void WaitForDevice();

} // namespace Graphics
