#include "Graphics.h"

#include "GraphicsUtils.h"
#include "LustraLib/LustraLib.h"
#include "SDL3/SDL_vulkan.h"
#include "SDLAssert.h"

#include <vector>

namespace
{
	// Should return bool that tells if the callback should be aborted. However, returning true is usually only for
	// testing the validation layers themselves, so returning false is the default behavior.
	VKAPI_ATTR VkBool32 VKAPI_CALL VkDebugMessagingCallback(
	    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	    VkDebugUtilsMessageTypeFlagsEXT messageType,
	    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	    void* userData
	)
	{
		UNUSED_VAR(userData);
		UNUSED_VAR(messageType);

		const bool prevSetting = LustraLib::gLoggerOptions.printSourceLocationInfo;

		// No need to print source location.
		LustraLib::gLoggerOptions.printSourceLocationInfo = false;

		const char* const message = callbackData->pMessage;
		if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			PRINT_ERROR("(Vulkan Error) {}", message);

			// Check stack trace for info on which call triggered the message.
			_DEBUG_TRAP();
		}
		else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			PRINT_WARNING("(Vulkan Warning) {}", message);
		}
		else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		{
			PRINT_DEBUG("(Vulkan Info) {}", message);
		}
		else
		{
			PRINT_LOG("(Vulkan Misc) {}", message);
		}

		LustraLib::gLoggerOptions.printSourceLocationInfo = prevSetting;

		return VK_FALSE;
	}

	const char* VkPhysicalDeviceTypeToString(VkPhysicalDeviceType deviceType)
	{
		switch (deviceType)
		{
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
				return "Integrated GPU";
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
				return "Discrete GPU";
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
				return "Virtual GPU";
			case VK_PHYSICAL_DEVICE_TYPE_CPU:
				return "CPU";
			default:
				return "Unknown Device Type";
		}
	}

	void PrintPhysicalDeviceProperties(const VkPhysicalDeviceProperties& properties)
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
} // namespace

namespace Graphics
{
	void SetupVulkan(std::string_view appName, const Window& window)
	{
		PRINT_DEBUG("Setting up Vulkan.");

		VkApplicationInfo applicationInfo  = vkInitStruct();
		applicationInfo.apiVersion         = gTargetVulkanVersion;
		applicationInfo.pApplicationName   = appName.empty() ? "Default App Name" : appName.data();
		applicationInfo.applicationVersion = gApplicationVersion;
		applicationInfo.pEngineName        = "Lustra Engine";
		applicationInfo.engineVersion      = gEngineVersion;

		SetupInstance(applicationInfo, ::GetSDLInstanceExtensions());
		SetupDevice();
		SetupDebugMessenger();
		SetupSwapchain(window);
	}

	void TearDownVulkan()
	{
		PRINT_DEBUG("Tearing down Vulkan.");

		if (gUseValidationLayers)
		{
			auto destroyDebugUtilsFunc = VK_LOAD_INSTANCE_FUNC(vkDestroyDebugUtilsMessengerEXT);
			ENSURE(destroyDebugUtilsFunc != nullptr);

			destroyDebugUtilsFunc(gVkInstance, gVkDebugMessenger, gVkAllocationCallbacks);
		}

		vkDestroySwapchainKHR(gVkDevice, gVkSwapchain, gVkAllocationCallbacks);
		vkDestroySurfaceKHR(gVkInstance, gVkSurface, gVkAllocationCallbacks);
		vkDestroyDevice(gVkDevice, gVkAllocationCallbacks);
		vkDestroyInstance(gVkInstance, gVkAllocationCallbacks);
	}

	void SetupInstance(
	    const VkApplicationInfo& vkApplicationInfo, const std::vector<const char*>& externalRequestedExtensions
	)
	{
		// NOTE: This is allowed to contain duplicate strings of the same extension.
		std::vector<const char*> instanceExtensions = {"VK_KHR_surface", "VK_KHR_get_surface_capabilities2"};
		for (const char* externalExtension : externalRequestedExtensions)
		{
			instanceExtensions.push_back(externalExtension);
		}

		std::vector<const char*> requestedLayers = {};

		if (gUseValidationLayers)
		{
			// Exposes an API specific for controlling debug utilities.
			instanceExtensions.push_back("VK_EXT_debug_utils");
			// Enables Vk functions to validate arguments against the spec.
			requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
		}

		// Check that requested validation layers are supported by the Vulkan instance.
		{
			uint32_t layerCount = 0;
			ASSERT_VK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

			std::vector<VkLayerProperties> instanceLayerProperties(layerCount);
			ASSERT_VK(vkEnumerateInstanceLayerProperties(&layerCount, instanceLayerProperties.data()));

			for (const char* requestedLayer : requestedLayers)
			{
				bool requestedLayerIsSupported = false;
				for (auto supportedLayer : instanceLayerProperties)
				{
					if (std::string_view(requestedLayer) == std::string_view(supportedLayer.layerName))
					{
						PRINT_DEBUG("Requested layer '{}' available.", requestedLayer);
						requestedLayerIsSupported = true;
						break;
					}
				}

				if (!requestedLayerIsSupported)
				{
					PRINT_ERROR("Requested layer '{}' was not found. Aborting.", requestedLayer);
					LUSTRA_ASSERT(false);
				}
			}
		}

		VkInstanceCreateInfo instanceCreateInfo    = vkInitStruct();
		instanceCreateInfo.pApplicationInfo        = &vkApplicationInfo;
		instanceCreateInfo.enabledExtensionCount   = instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
		instanceCreateInfo.enabledLayerCount       = requestedLayers.size();
		instanceCreateInfo.ppEnabledLayerNames     = requestedLayers.data();

		if (gUseValidationLayers)
		{
			// Will create a debug messenger scoped to only instance creation and instance destruction calls.
			VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = vkInitStruct();
			debugMessengerCreateInfo.messageSeverity =
			    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
			debugMessengerCreateInfo.messageType =
			    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
			debugMessengerCreateInfo.pfnUserCallback = ::VkDebugMessagingCallback;

			instanceCreateInfo.pNext = &debugMessengerCreateInfo;
		}

		ASSERT_VK(vkCreateInstance(&instanceCreateInfo, gVkAllocationCallbacks, &gVkInstance));
	}

	void SetupDevice()
	{
		uint32_t deviceCount = 0;
		ASSERT_VK(vkEnumeratePhysicalDevices(gVkInstance, &deviceCount, nullptr));

		ENSURE_EX(deviceCount != 0, "Could not find a GPU with Vulkan support.");

		std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
		ASSERT_VK(vkEnumeratePhysicalDevices(gVkInstance, &deviceCount, physicalDevices.data()));

		VkPhysicalDeviceProperties chosenPhysicalDeviceProperties = {};
		for (VkPhysicalDevice physicalDevice : physicalDevices)
		{
			VkPhysicalDeviceFeatures features     = {};
			VkPhysicalDeviceProperties properties = {};

			vkGetPhysicalDeviceProperties(physicalDevice, &properties);
			vkGetPhysicalDeviceFeatures(physicalDevice, &features);

			// Always print list of physical devices available to get an overview of the system.
			::PrintPhysicalDeviceProperties(properties);

			// Only check for suitable device if none has been found yet.
			if (gVkPhysicalDevice == VK_NULL_HANDLE)
			{
				// Assume to be true and go through checks to verify.
				bool isSuitableDevice = true;

				isSuitableDevice &= (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

				if (isSuitableDevice)
				{
					gVkPhysicalDevice              = physicalDevice;
					chosenPhysicalDeviceProperties = properties;
				}
			}
		}

		ENSURE_EX(gVkPhysicalDevice != VK_NULL_HANDLE, "Could not find a suitable GPU.");

		PRINT_LOG("Chosen physical device: '{}'", chosenPhysicalDeviceProperties.deviceName);

		VkPhysicalDeviceFeatures2 requestedDeviceFeatures = vkInitStruct();
		// Check if requested device features are available
		{
			VkPhysicalDeviceFeatures2 availableDeviceFeatures = vkInitStruct();
			vkGetPhysicalDeviceFeatures2(gVkPhysicalDevice, &availableDeviceFeatures);

			PRINT_LOG("====== DEVICE FEATURES ======");

			const VkBool32* requestedDeviceFeaturesArr = reinterpret_cast<VkBool32*>(&requestedDeviceFeatures.features);
			const VkBool32* availableDeviceFeaturesArr = reinterpret_cast<VkBool32*>(&availableDeviceFeatures.features);
			bool requestedFeaturesMissing              = false;
			for (size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures2::features) / sizeof(VkBool32); i++)
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

				PRINT_LOG("[{:^10}] {}", featureSupport, VkDeviceFeatureToString(i));

				requestedFeaturesMissing |= featureIsMissing;
			}

			ENSURE_EX(requestedFeaturesMissing == false, "Device does not support all features that were requested.");

			PRINT_LOG("=============================");
		}

		std::vector<const char*> const requestedDeviceExtensions = {"VK_KHR_swapchain"};

		// Check if requested device extensions are available
		{
			uint32_t extensionCount = 0;
			ASSERT_VK(vkEnumerateDeviceExtensionProperties(gVkPhysicalDevice, nullptr, &extensionCount, nullptr));

			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			ASSERT_VK(vkEnumerateDeviceExtensionProperties(
			    gVkPhysicalDevice, nullptr, &extensionCount, availableExtensions.data()
			));

			for (const char* requestedExtensionName : requestedDeviceExtensions)
			{
				bool requestedExtensionFound = false;
				for (const VkExtensionProperties& availableExtension : availableExtensions)
				{
					if (std::string_view(availableExtension.extensionName) == std::string_view(requestedExtensionName))
					{
						requestedExtensionFound = true;
						break;
					}
				}

				if (!requestedExtensionFound)
				{
					PRINT_ERROR("Requested device extension '{}' was not found.", requestedExtensionName);
					LUSTRA_ASSERT(false);
				}
			}
		}

		const float priority                    = 1.0f;
		VkDeviceQueueCreateInfo queueCreateInfo = vkInitStruct();
		queueCreateInfo.queueCount              = 1;
		queueCreateInfo.queueFamilyIndex        = 0;
		queueCreateInfo.pQueuePriorities        = &priority;

		// Only use device features and device extensions that are requested.
		VkDeviceCreateInfo deviceCreateInfo      = vkInitStruct();
		deviceCreateInfo.queueCreateInfoCount    = 1;
		deviceCreateInfo.pQueueCreateInfos       = &queueCreateInfo;
		deviceCreateInfo.pNext                   = &requestedDeviceFeatures;
		deviceCreateInfo.enabledExtensionCount   = requestedDeviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = requestedDeviceExtensions.data();

		ASSERT_VK(vkCreateDevice(gVkPhysicalDevice, &deviceCreateInfo, gVkAllocationCallbacks, &gVkDevice));

		// Search for common queue families (only after digital device has been created)
		{
			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties2(gVkPhysicalDevice, &queueFamilyCount, nullptr);

			std::vector<VkQueueFamilyProperties2> queueFamilyProperties = vkInitStructs(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties2(
			    gVkPhysicalDevice, &queueFamilyCount, queueFamilyProperties.data()
			);

			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
			{
				const VkQueueFlags queueFlags = queueFamilyProperties[i].queueFamilyProperties.queueFlags;

				const bool hasGraphicsBit = (queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u;
				const bool hasComputeBit  = (queueFlags & VK_QUEUE_COMPUTE_BIT) != 0u;
				const bool hasTransferBit = (queueFlags & VK_QUEUE_TRANSFER_BIT) != 0u;

				if (hasGraphicsBit && !gQueueFamilyIndices.graphics.has_value())
				{
					gQueueFamilyIndices.graphics = i;
				}

				if (hasComputeBit && !hasGraphicsBit && !gQueueFamilyIndices.compute.has_value())
				{
					gQueueFamilyIndices.compute = i;
				}

				if (hasTransferBit && !(hasComputeBit || hasGraphicsBit) && !gQueueFamilyIndices.transfer.has_value())
				{
					gQueueFamilyIndices.transfer = i;
				}
			}

			// Check that all queues were found.
			CHECK(gQueueFamilyIndices.graphics.has_value());
			CHECK(gQueueFamilyIndices.compute.has_value());
			CHECK(gQueueFamilyIndices.transfer.has_value());
		}
	}

	void SetupDebugMessenger()
	{
		if (!gUseValidationLayers)
		{
			return;
		}

		// TODO: Could be extended to have options for all flags at setup through some global settings struct.
		VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = vkInitStruct();
		debugMessengerCreateInfo.messageSeverity =
		    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
		debugMessengerCreateInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		                                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
		                                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debugMessengerCreateInfo.pfnUserCallback = ::VkDebugMessagingCallback;
		debugMessengerCreateInfo.pUserData       = nullptr; // No user data for now.

		auto debugUtilsFunc = VK_LOAD_INSTANCE_FUNC(vkCreateDebugUtilsMessengerEXT);
		ENSURE_EX(debugUtilsFunc != nullptr, "Could not load debug utils EXT.");

		ASSERT_VK(debugUtilsFunc(gVkInstance, &debugMessengerCreateInfo, gVkAllocationCallbacks, &gVkDebugMessenger));
	}

	void SetupSwapchain(const Window& window)
	{
		auto* sdlWindow = reinterpret_cast<SDL_Window*>(window.GetWindow());
		ASSERT_SDL(
		    SDL_Vulkan_CreateSurface(sdlWindow, gVkInstance, gVkAllocationCallbacks, &gVkSurface),
		    "Could not create Vulkan surface"
		);

		// Find what surface formats the physical device supports
		{
			VkPhysicalDeviceSurfaceInfo2KHR physicalDeviceSurfaceInfo = vkInitStruct();
			physicalDeviceSurfaceInfo.surface                         = gVkSurface;

			uint32_t surfaceCount;
			ASSERT_VK(vkGetPhysicalDeviceSurfaceFormats2KHR(
			    gVkPhysicalDevice, &physicalDeviceSurfaceInfo, &surfaceCount, nullptr
			));

			std::vector<VkSurfaceFormat2KHR> surfaceFormats = vkInitStructs(surfaceCount);
			ASSERT_VK(vkGetPhysicalDeviceSurfaceFormats2KHR(
			    gVkPhysicalDevice, &physicalDeviceSurfaceInfo, &surfaceCount, surfaceFormats.data()
			));

			bool foundRequestedSurfaceFormat = false;
			for (const VkSurfaceFormat2KHR& availableSurfaceFormat : surfaceFormats)
			{
				// Do the structs contain the exact same data?
				if (std::memcmp(
				        &availableSurfaceFormat.surfaceFormat, &gTargetSurfaceFormat, sizeof(VkSurfaceFormatKHR)
				    ) == 0)
				{
					foundRequestedSurfaceFormat = true;
					break;
				}
			}

			ENSURE_EX(foundRequestedSurfaceFormat, "Physical device does not support requested surface format");
		}

		VkExtent2D imageExtent = {};
		window.GetExtentInPixels(imageExtent.width, imageExtent.height);

		VkSwapchainCreateInfoKHR swapchainCreateInfo = vkInitStruct();
		swapchainCreateInfo.surface                  = gVkSurface;
		swapchainCreateInfo.minImageCount            = gSwapchainImages.size();
		swapchainCreateInfo.imageFormat              = gTargetSurfaceFormat.format;
		swapchainCreateInfo.imageColorSpace          = gTargetSurfaceFormat.colorSpace;
		swapchainCreateInfo.imageExtent              = imageExtent;
		swapchainCreateInfo.imageArrayLayers         = 1;
		swapchainCreateInfo.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainCreateInfo.presentMode              = VK_PRESENT_MODE_IMMEDIATE_KHR;
		swapchainCreateInfo.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.preTransform             = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		swapchainCreateInfo.compositeAlpha           = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.clipped                  = VK_TRUE;

		ASSERT_VK(vkCreateSwapchainKHR(gVkDevice, &swapchainCreateInfo, gVkAllocationCallbacks, &gVkSwapchain));

		uint32_t imageCount = gSwapchainImages.size();
		ASSERT_VK(vkGetSwapchainImagesKHR(gVkDevice, gVkSwapchain, &imageCount, gSwapchainImages.data()));
	}
} // namespace Graphics
