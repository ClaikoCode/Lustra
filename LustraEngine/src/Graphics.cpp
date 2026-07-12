#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VMA_STATIC_VULKAN_FUNCTIONS 0  // no static symbols exist
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1 // let VMA fetch the rest itself
#define VMA_IMPLEMENTATION
#include "Graphics.h"

#include "GraphicsUtils.h"
#include "LustraLib/LustraLib.h"
#include "SDL3/SDL_vulkan.h"
#include "SDLAssert.h"

#include <algorithm>
#include <vector>

// Single global default dispatcher storage. Should only pertain to a single TU.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

// Homeless resources
// TODO: Move to separate files later

namespace
{
	// Should return bool that tells if the callback should be aborted. However, returning true is usually only for
	// testing the validation layers themselves, so returning false is the default behavior.
	vk::Bool32 VkDebugMessagingCallback(
	    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	    vk::DebugUtilsMessageTypeFlagsEXT messageType,
	    const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
	    void* userData
	)
	{
		UNUSED_VAR(userData);
		UNUSED_VAR(messageType);

		LOGGER_DISABLE_LOCATION();

		const char* const message = callbackData->pMessage;
		if (messageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
		{
			PRINT_ERROR("(Vulkan Error) {}", message);

			// Check stack trace for info on which call triggered the message.
			LUSTRA_ASSERT(false);
		}
		else if (messageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
		{
			const bool isBestPracticeMessage =
			    callbackData->pMessageIdName != nullptr &&
			    std::string_view(callbackData->pMessageIdName).starts_with("BestPractices-");

			const std::string_view warningMessagePrefix =
			    isBestPracticeMessage ? "(Vulkan Best Practice)" : "(Vulkan Warning)";

			PRINT_WARNING("{} {}", warningMessagePrefix, message);
		}
		else if (messageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
		{
			PRINT_DEBUG("(Vulkan Info) {}", message);
		}
		else
		{
			PRINT_LOG("(Vulkan Misc) {}", message);
		}

		LOGGER_RESTORE_LOCATION();

		return vk::False;
	}

	// Gives friendly name for device type.
	// Already exists in vk::to_string() but I like my strings better.
	const char* VkPhysicalDeviceTypeToString(vk::PhysicalDeviceType deviceType)
	{
		switch (deviceType)
		{
			case vk::PhysicalDeviceType::eIntegratedGpu:
				return "Integrated GPU";
			case vk::PhysicalDeviceType::eDiscreteGpu:
				return "Discrete GPU";
			case vk::PhysicalDeviceType::eVirtualGpu:
				return "Virtual GPU";
			case vk::PhysicalDeviceType::eCpu:
				return "CPU";
			default:
				return "Unknown Device Type";
		}
	}

	void PrintPhysicalDeviceProperties(const vk::PhysicalDeviceProperties& properties)
	{
		constexpr const char* formatString = "\n==== PHYSICAL DEVICE INFO ====\n"
		                                     "Device Name: 		{}\n"
		                                     "Device Type:		{}\n"
		                                     "API Version: 		{}.{}.{}\n"
		                                     "Driver Version: 	{}.{}.{}\n"
		                                     "Vendor ID:		{}\n"
		                                     "Device ID:		{}\n"
		                                     "==============================";

		uint32_t vkAPIMajor = VK_VERSION_MAJOR(properties.apiVersion);
		uint32_t vkAPIMinor = VK_VERSION_MINOR(properties.apiVersion);
		uint32_t vkAPIPatch = VK_VERSION_PATCH(properties.apiVersion);

		uint32_t driverMajor = VK_VERSION_MAJOR(properties.driverVersion);
		uint32_t driverMinor = VK_VERSION_MINOR(properties.driverVersion);
		uint32_t driverPatch = VK_VERSION_PATCH(properties.driverVersion);

		PRINT_LOG(
		    formatString,
		    static_cast<const char*>(properties.deviceName),
		    ::VkPhysicalDeviceTypeToString(properties.deviceType),
		    vkAPIMajor,
		    vkAPIMinor,
		    vkAPIPatch,
		    driverMajor,
		    driverMinor,
		    driverPatch,
		    properties.vendorID,
		    properties.deviceID
		);
	}

	std::vector<const char*> GetSDLInstanceExtensions()
	{
		uint32_t sdlInstanceExtensionsCount      = 0;
		const char* const* sdlInstanceExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlInstanceExtensionsCount);
		ASSERT_SDL(sdlInstanceExtensions != nullptr, "Unable to get Vulkan instance extensions required by SDL");

		std::vector<const char*> externalRequestedExtensions(sdlInstanceExtensionsCount);
		for (uint32_t i = 0; i < sdlInstanceExtensionsCount; i++)
		{
			externalRequestedExtensions[i] = sdlInstanceExtensions[i];
		}

		return externalRequestedExtensions;
	}

	bool GetSDLPresentationSupport(uint32_t queueFamilyIndex)
	{
		return SDL_Vulkan_GetPresentationSupport(Graphics::gVkInstance, Graphics::gVkPhysicalDevice, queueFamilyIndex);
	}

} // namespace

namespace Graphics
{
	void SetupVulkan(std::string_view appName, const Window& window)
	{
		PRINT_DEBUG("Setting up Vulkan.");

		const vk::ApplicationInfo applicationInfo = {
		    .pApplicationName   = appName.empty() ? "Default App Name" : appName.data(),
		    .applicationVersion = gApplicationVersion,
		    .pEngineName        = "Lustra Engine",
		    .engineVersion      = gEngineVersion,
		    .apiVersion         = gTargetVulkanVersion,
		};

		// Store a pointer to the window
		gWindowPtr = &window;

		// Setup dynamic loader.
		const vk::detail::DynamicLoader dl;
		auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
		vk::detail::defaultDispatchLoaderDynamic.init(vkGetInstanceProcAddr);

		// Start initialization of all core systems.
		SetupInstance(applicationInfo, ::GetSDLInstanceExtensions());
		SetupDebugMessenger();
		SetupDevice();
		SetupVMA();
		SetupSurface(window);
		CreateSwapchain(window);
	}

	void TearDownVulkan()
	{
		PRINT_DEBUG("Tearing down Vulkan.");

		if constexpr (gUseValidationLayers)
		{
			gVkInstance.destroyDebugUtilsMessengerEXT(gVkDebugMessenger, gAllocationCallbacks);
		}

		gSwapchain.Destroy();
		gVkInstance.destroySurfaceKHR(gVkSurface, gAllocationCallbacks);
		vmaDestroyAllocator(gVmaAllocator);
		gVkDevice.destroy(gAllocationCallbacks);
		gVkInstance.destroy(gAllocationCallbacks);
	}

	void SetupInstance(
	    const vk::ApplicationInfo& vkApplicationInfo, const std::vector<const char*>& externalRequestedExtensions
	)
	{
		std::vector<const char*> requestedExtensions = {"VK_KHR_surface", "VK_KHR_get_surface_capabilities2"};
		std::vector<const char*> requestedLayers     = {};

		if constexpr (gUseValidationLayers)
		{
			// Exposes an API specific for controlling debug utilities.
			requestedExtensions.push_back("VK_EXT_debug_utils");
			// Enables Vk functions to validate arguments against the spec.
			requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
		}

		// NOTE: Vulkan allows this list to contain duplicate strings of the same extension but this is done for
		// cleanliness and debug output purposes.
		for (const char* externalExtension : externalRequestedExtensions)
		{
			const bool alreadyExistsInRequestedExtensions =
			    std::ranges::contains(requestedExtensions, std::string_view(externalExtension));

			if (!alreadyExistsInRequestedExtensions)
			{
				requestedExtensions.push_back(externalExtension);
			}
		}

		// Check that requested validation layers are supported by the Vulkan instance.
		{
			const std::vector<vk::LayerProperties> supportedLayers = AssertVk(vk::enumerateInstanceLayerProperties());

			for (const char* requestedLayer : requestedLayers)
			{
				bool requestedLayerIsSupported = false;
				for (auto supportedLayer : supportedLayers)
				{
					if (std::string_view(requestedLayer) == std::string_view(supportedLayer.layerName))
					{
						PRINT_DEBUG("Requested layer '{}' available.", requestedLayer);
						requestedLayerIsSupported = true;
						break;
					}
				}

				ENSURE_EX(requestedLayerIsSupported, "Requested layer '{}' was not found.", requestedLayer);
			}
		}

		// Check the same for requested extensions.
		{
			const std::vector<vk::ExtensionProperties> supportedExtensions =
			    AssertVk(vk::enumerateInstanceExtensionProperties());

			for (const char* requestedExtension : requestedExtensions)
			{
				bool requestedExtensionIsSupported = false;
				for (auto supportedExtension : supportedExtensions)
				{
					if (std::string_view(requestedExtension) == std::string_view(supportedExtension.extensionName))
					{
						PRINT_DEBUG("Requested extension '{}' available.", requestedExtension);
						requestedExtensionIsSupported = true;
						break;
					}
				}

				ENSURE_EX(requestedExtensionIsSupported, "Requested extension '{}' was not found.", requestedExtension);
			}
		}

		vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo        = {};
		std::vector<vk::ValidationFeatureEnableEXT> validationFeatureEnables = {
		    vk::ValidationFeatureEnableEXT::eBestPractices
		};

		vk::InstanceCreateInfo instanceCreateInfo{
		    .pApplicationInfo = &vkApplicationInfo,
		};
		instanceCreateInfo.setPEnabledExtensionNames(requestedExtensions);
		instanceCreateInfo.setPEnabledLayerNames(requestedLayers);

		// Will create a debug messenger scoped to only instance creation and instance destruction calls.
		vk::ValidationFeaturesEXT validationFeatures = {};
		if constexpr (gUseValidationLayers)
		{
			// Debug messenger create info
			debugMessengerCreateInfo.messageSeverity =
			    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
			debugMessengerCreateInfo.messageType =
			    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
			debugMessengerCreateInfo.pfnUserCallback = ::VkDebugMessagingCallback;

			validationFeatures.setEnabledValidationFeatures(validationFeatureEnables);

			validationFeatures.pNext = &debugMessengerCreateInfo;
			instanceCreateInfo.pNext = &validationFeatures;
		}

		gVkInstance = AssertVk(vk::createInstance(instanceCreateInfo, gAllocationCallbacks));

		// Load instance level functions.
		vk::detail::defaultDispatchLoaderDynamic.init(gVkInstance);
	}

	void SetupDevice()
	{
		vk::PhysicalDeviceProperties chosenPhysicalDeviceProperties = {};

		// Find a suitable physical device
		{
			const std::vector<vk::PhysicalDevice> physicalDevices = AssertVk(gVkInstance.enumeratePhysicalDevices());
			ENSURE_EX(!physicalDevices.empty(), "Could not find a GPU with Vulkan support.");

			for (const vk::PhysicalDevice physicalDevice : physicalDevices)
			{
				vk::PhysicalDeviceFeatures features     = {};
				vk::PhysicalDeviceProperties properties = {};

				physicalDevice.getFeatures(&features);
				physicalDevice.getProperties(&properties);

				// Always print list of physical devices available to get an overview of the system.
				::PrintPhysicalDeviceProperties(properties);

				// Only check for suitable device if none has been found yet.
				if (gVkPhysicalDevice == VK_NULL_HANDLE)
				{
					// Assume to be true and go through checks to verify.
					bool isSuitableDevice = true;

					isSuitableDevice &= (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu);

					if (isSuitableDevice)
					{
						gVkPhysicalDevice              = physicalDevice;
						chosenPhysicalDeviceProperties = properties;
					}
				}
			}

			ENSURE_EX(gVkPhysicalDevice != VK_NULL_HANDLE, "Could not find a suitable GPU.");

			std::string_view deviceName = chosenPhysicalDeviceProperties.deviceName;
			PRINT_LOG("Chosen physical device: '{}'", deviceName);
		}

		GraphicsUtils::FeatureChain requestedFeatureChain = {};

		requestedFeatureChain.get<vk::PhysicalDeviceVulkan12Features>().setTimelineSemaphore(VK_TRUE);

		requestedFeatureChain.get<vk::PhysicalDeviceVulkan13Features>()
		    .setSynchronization2(VK_TRUE)
		    .setDynamicRendering(VK_TRUE);

		// Check if requested device features are available
		{
			GraphicsUtils::FeatureChain availableFeatureChain = {};
			gVkPhysicalDevice.getFeatures2(&availableFeatureChain.get<vk::PhysicalDeviceFeatures2KHR>());

			PRINT_LOG("====== DEVICE FEATURES ======");

			// Cast to Base structure that only contain type and pnext.
			const auto* currentAvailableFeature = reinterpret_cast<const vk::BaseOutStructure*>(
			    &availableFeatureChain.get<vk::PhysicalDeviceFeatures2KHR>()
			);
			const auto* currentRequestedFeature = reinterpret_cast<const vk::BaseOutStructure*>(
			    &requestedFeatureChain.get<vk::PhysicalDeviceFeatures2KHR>()
			);

			bool requestedFeaturesMissing = false;
			// Loop through linked list.
			while (currentAvailableFeature != nullptr)
			{
				std::string_view vulkanVersionString;
				switch (currentAvailableFeature->sType)
				{
					case vk::StructureType::ePhysicalDeviceFeatures2:
						vulkanVersionString = "1.0";
						break;
					case vk::StructureType::ePhysicalDeviceVulkan11Features:
						vulkanVersionString = "1.1";
						break;
					case vk::StructureType::ePhysicalDeviceVulkan12Features:
						vulkanVersionString = "1.2";
						break;
					case vk::StructureType::ePhysicalDeviceVulkan13Features:
						vulkanVersionString = "1.3";
						break;
					case vk::StructureType::ePhysicalDeviceVulkan14Features:
						vulkanVersionString = "1.4";
						break;
					default:
						CHECK_UNREACHABLE();
				}

				PRINT_LOG(" -- Vulkan {} -- ", vulkanVersionString);

				const GraphicsUtils::FeatureNamesInfo featuresInfo =
				    GraphicsUtils::GetFeatureNames(currentAvailableFeature->sType);
				const uint16_t firstFeatureFieldOffset = featuresInfo.firstOffset;

				// Because all features are set in stone and are a series of VkBool32's, they can be linearly iterated.
				const auto* requestedDeviceFeaturesArr = reinterpret_cast<const VkBool32*>(
				    reinterpret_cast<const char*>(currentRequestedFeature) + firstFeatureFieldOffset
				);

				const auto* availableDeviceFeaturesArr = reinterpret_cast<const VkBool32*>(
				    reinterpret_cast<const char*>(currentAvailableFeature) + firstFeatureFieldOffset
				);

				for (uint16_t i = 0; i < featuresInfo.count; i++)
				{
					bool featureIsMissing      = false;
					const char* featureSupport = "NOT USED";
					if (requestedDeviceFeaturesArr[i] == VK_TRUE)
					{
						if (availableDeviceFeaturesArr[i] == VK_FALSE)
						{
							featureIsMissing = true;

							featureSupport = "MISSING";
						}
						else
						{
							featureSupport = "FOUND";
						}
					}

					PRINT_LOG("[{:^10}] {}", featureSupport, featuresInfo.names[i]);

					requestedFeaturesMissing |= featureIsMissing;
				}

				currentAvailableFeature = currentAvailableFeature->pNext;
				currentRequestedFeature = currentRequestedFeature->pNext;
			}

			ENSURE_EX(requestedFeaturesMissing == false, "Device does not support all features that were requested.");

			PRINT_LOG("=============================");
		}

		// Search for common queue families available to the physical device
		{
			std::vector<vk::QueueFamilyProperties2> queueFamilyProperties =
			    gVkPhysicalDevice.getQueueFamilyProperties2();

			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
			{
				const vk::QueueFlags queueFlags = queueFamilyProperties[i].queueFamilyProperties.queueFlags;

				const bool hasGraphicsBit = static_cast<bool>(queueFlags & vk::QueueFlagBits::eGraphics);
				const bool hasComputeBit  = static_cast<bool>(queueFlags & vk::QueueFlagBits::eCompute);
				const bool hasTransferBit = static_cast<bool>(queueFlags & vk::QueueFlagBits::eTransfer);

				if (hasGraphicsBit && hasComputeBit && graphicsQueue.index == UINT32_MAX)
				{
					if (::GetSDLPresentationSupport(i))
					{
						graphicsQueue.index = i;
					}
				}

				if (hasComputeBit && !hasGraphicsBit && computeQueue.index == UINT32_MAX)
				{
					computeQueue.index = i;
				}

				if (hasTransferBit && !(hasComputeBit || hasGraphicsBit) && transferQueue.index == UINT32_MAX)
				{
					transferQueue.index = i;
				}
			}

			// Ensure that all queues were found.
			ENSURE(graphicsQueue.index != UINT32_MAX);
			ENSURE(computeQueue.index != UINT32_MAX);
			ENSURE(transferQueue.index != UINT32_MAX);
		}

		const std::vector<const char*> requestedDeviceExtensions = {"VK_KHR_swapchain"};

		// Check if requested device extensions are available
		{
			const std::vector<vk::ExtensionProperties> availableExtensions =
			    AssertVk(gVkPhysicalDevice.enumerateDeviceExtensionProperties());

			for (const char* requestedExtensionName : requestedDeviceExtensions)
			{
				bool requestedExtensionFound = false;
				for (const vk::ExtensionProperties& availableExtension : availableExtensions)
				{
					if (std::string_view(availableExtension.extensionName) == std::string_view(requestedExtensionName))
					{
						requestedExtensionFound = true;
						break;
					}
				}

				ENSURE_EX(
				    requestedExtensionFound, "Requested device extension '{}' was not found.", requestedExtensionName
				);
			}
		}

		const float priority                                           = 1.0f;
		const std::array<vk::DeviceQueueCreateInfo, 3> queueCreateInfo = {
		    vk::DeviceQueueCreateInfo{
		        .queueFamilyIndex = graphicsQueue.index, .queueCount = 1, .pQueuePriorities = &priority
		    },
		    vk::DeviceQueueCreateInfo{
		        .queueFamilyIndex = computeQueue.index, .queueCount = 1, .pQueuePriorities = &priority
		    },
		    vk::DeviceQueueCreateInfo{
		        .queueFamilyIndex = transferQueue.index, .queueCount = 1, .pQueuePriorities = &priority
		    }
		};

		// Only use device features and device extensions that are requested.
		vk::DeviceCreateInfo deviceCreateInfo = {
		    .pNext = &requestedFeatureChain.get<vk::PhysicalDeviceFeatures2KHR>(),
		};
		deviceCreateInfo.setQueueCreateInfos(queueCreateInfo);
		deviceCreateInfo.setPEnabledExtensionNames(requestedDeviceExtensions);

		gVkDevice = AssertVk(gVkPhysicalDevice.createDevice(deviceCreateInfo, gAllocationCallbacks));

		// Load device level functions.
		vk::detail::defaultDispatchLoaderDynamic.init(gVkDevice);

		// Get the queues after device has been created.
		graphicsQueue.queue = gVkDevice.getQueue(graphicsQueue.index, 0);
		computeQueue.queue  = gVkDevice.getQueue(computeQueue.index, 0);
		transferQueue.queue = gVkDevice.getQueue(transferQueue.index, 0);
	}

	void SetupDebugMessenger()
	{
		if constexpr (!gUseValidationLayers)
		{
			return;
		}

		// TODO: Could be extended to have options for all flags at setup through some global settings struct.
		const vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = {
		    .messageSeverity =
		        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		        vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
		    .messageType     = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		                       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
		    .pfnUserCallback = ::VkDebugMessagingCallback,
		    .pUserData       = nullptr // No user data for now
		};

		gVkDebugMessenger =
		    AssertVk(gVkInstance.createDebugUtilsMessengerEXT(debugMessengerCreateInfo, gAllocationCallbacks));
	}

	void SetupSurface(const Window& window)
	{
		auto* sdlWindow         = static_cast<SDL_Window*>(window.GetWindow());
		VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
		ASSERT_SDL(
		    SDL_Vulkan_CreateSurface(
		        sdlWindow,
		        gVkInstance,
		        reinterpret_cast<const VkAllocationCallbacks*>(gAllocationCallbacks),
		        &rawSurface
		    ),
		    "Could not create Vulkan surface"
		);

		gVkSurface = vk::SurfaceKHR(rawSurface);

		// Check available presentation modes
		{
			const std::vector<vk::PresentModeKHR> presentationModes =
			    AssertVk(gVkPhysicalDevice.getSurfacePresentModesKHR(gVkSurface));

			// Standard presentation modes.
			const std::vector<vk::PresentModeKHR> requestedPresentationModes = {
			    vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eMailbox
			};

			for (const vk::PresentModeKHR requestedPresentMode : requestedPresentationModes)
			{
				bool foundPresentMode = false;

				for (const vk::PresentModeKHR availablePresentMode : presentationModes)
				{
					if (requestedPresentMode == availablePresentMode)
					{
						foundPresentMode = true;
						break;
					}
				}

				ENSURE_EX(
				    foundPresentMode,
				    "Could not find requested presentation mode: {}",
				    vk::to_string(requestedPresentMode)
				);
			}
		}
	}

	void CreateSwapchain(const Window& window)
	{
		// Find surface capabilities and see if swapchain can support our desired format.
		vk::SurfaceCapabilities2KHR surfaceCapabilities2 = {};
		{
			const vk::PhysicalDeviceSurfaceInfo2KHR physicalDeviceSurfaceInfo = {.surface = gVkSurface};

			const std::vector<vk::SurfaceFormat2KHR> surfaceFormats =
			    AssertVk(gVkPhysicalDevice.getSurfaceFormats2KHR(physicalDeviceSurfaceInfo));

			bool foundRequestedSurfaceFormat = false;
			for (const vk::SurfaceFormat2KHR& availableSurfaceFormat : surfaceFormats)
			{
				const vk::SurfaceFormatKHR surfaceFormat = availableSurfaceFormat.surfaceFormat;

				const bool sameFormat     = surfaceFormat.format == gTargetSurfaceFormat.format;
				const bool sameColorSpace = surfaceFormat.colorSpace == gTargetSurfaceFormat.colorSpace;

				if (sameFormat && sameColorSpace)
				{
					foundRequestedSurfaceFormat = true;
					break;
				}
			}

			ENSURE_EX(
			    foundRequestedSurfaceFormat,
			    "Physical device does not support requested surface format: {}",
			    vk::to_string(gTargetSurfaceFormat.format)
			);

			surfaceCapabilities2 = AssertVk(gVkPhysicalDevice.getSurfaceCapabilities2KHR(physicalDeviceSurfaceInfo));
		}

		const vk::SurfaceCapabilitiesKHR surfaceCapabilities = surfaceCapabilities2.surfaceCapabilities;

		// If using Wayland then extent is decided from window.
		if (surfaceCapabilities2.surfaceCapabilities.currentExtent.width == 0xFFFFFFFF)
		{
			window.GetExtentInPixels(gSwapchain.width, gSwapchain.height);
		}
		else
		{
			gSwapchain.width  = surfaceCapabilities.currentExtent.width;
			gSwapchain.height = surfaceCapabilities.currentExtent.height;
		}

		// Makes sure requested image count is guranteed to be between 2 and max supported image count.
		const uint32_t requestedImageCount =
		    std::min(surfaceCapabilities.maxImageCount, std::max(2u, surfaceCapabilities.minImageCount));

		const vk::SwapchainCreateInfoKHR swapchainCreateInfo = {
		    .surface          = gVkSurface,
		    .minImageCount    = requestedImageCount,
		    .imageFormat      = gTargetSurfaceFormat.format,
		    .imageColorSpace  = gTargetSurfaceFormat.colorSpace,
		    .imageExtent      = {.width = gSwapchain.width, .height = gSwapchain.height},
		    .imageArrayLayers = 1,
		    .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
		    .imageSharingMode = vk::SharingMode::eExclusive,
		    .preTransform     = surfaceCapabilities.currentTransform, // Keep the transform of the users display.
		    .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		    .presentMode      = vk::PresentModeKHR::eImmediate,
		    .clipped          = VK_TRUE
		};

		gSwapchain.swapchain = AssertVk(gVkDevice.createSwapchainKHR(swapchainCreateInfo, gAllocationCallbacks));

		gSwapchain.images                = AssertVk(gVkDevice.getSwapchainImagesKHR(gSwapchain.swapchain));
		const size_t swapChainImageCount = gSwapchain.images.size();

		gSwapchain.views.resize(swapChainImageCount);
		for (size_t i = 0; i < swapChainImageCount; i++)
		{
			const vk::ImageViewCreateInfo imgViewInfo = {
			    .image            = gSwapchain.images[i],
			    .viewType         = vk::ImageViewType::e2D,
			    .format           = gTargetSurfaceFormat.format,
			    .subresourceRange = {
			        .aspectMask     = vk::ImageAspectFlagBits::eColor,
			        .baseMipLevel   = 0,
			        .levelCount     = 1,
			        .baseArrayLayer = 0,
			        .layerCount     = 1
			    }
			};

			gSwapchain.views[i] = AssertVk(gVkDevice.createImageView(imgViewInfo, gAllocationCallbacks));
		}

		gSwapchain.semaphores.resize(swapChainImageCount);
		for (vk::Semaphore& semaphore : gSwapchain.semaphores)
		{
			const vk::SemaphoreCreateInfo semaphoreInfo = {};

			semaphore = AssertVk(gVkDevice.createSemaphore(semaphoreInfo, gAllocationCallbacks));
		}
	}

	void SetupVMA()
	{
		const auto& dispatcher = VULKAN_HPP_DEFAULT_DISPATCHER;

		VmaVulkanFunctions vmaFuncInfo    = {};
		vmaFuncInfo.vkGetInstanceProcAddr = dispatcher.vkGetInstanceProcAddr;
		vmaFuncInfo.vkGetDeviceProcAddr   = dispatcher.vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo vmaAllocInfo = {};
		vmaAllocInfo.flags                  = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		vmaAllocInfo.physicalDevice         = gVkPhysicalDevice;
		vmaAllocInfo.device                 = gVkDevice;
		vmaAllocInfo.pVulkanFunctions       = &vmaFuncInfo;
		vmaAllocInfo.instance               = gVkInstance;
		vmaAllocInfo.vulkanApiVersion       = gTargetVulkanVersion;
		vmaAllocInfo.pAllocationCallbacks   = reinterpret_cast<const VkAllocationCallbacks*>(gAllocationCallbacks);

		AssertVk(static_cast<vk::Result>(vmaCreateAllocator(&vmaAllocInfo, &gVmaAllocator)));
	}

	void WaitForDevice()
	{
		AssertVk(gVkDevice.waitIdle());
	}
} // namespace Graphics

// Struct definitions
namespace Graphics
{
	void Swapchain::Destroy()
	{
		VALIDATE_EX(gVkDevice != VK_NULL_HANDLE, "Attempted to destroy swapchain without an existing device.");

		for (const vk::ImageView& imageView : views)
		{
			gVkDevice.destroyImageView(imageView, gAllocationCallbacks);
		}
		views.clear();

		for (const vk::Semaphore& semaphore : semaphores)
		{
			gVkDevice.destroySemaphore(semaphore, gAllocationCallbacks);
		}
		semaphores.clear();

		if (swapchain)
		{
			gVkDevice.destroySwapchainKHR(swapchain, gAllocationCallbacks);

			// Swapchain also destroys images that came with it but clear the vector for cleanliness.
			images.clear();
		}
	}
} // namespace Graphics
