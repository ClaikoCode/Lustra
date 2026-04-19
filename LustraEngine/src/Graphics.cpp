#include "Graphics.h"

#include "GraphicsUtils.h"
#include "LustraLib/LustraLib.h"

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
		    properties.deviceName,
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

} // namespace

namespace Graphics
{
	void SetupVulkan(const std::string_view appName, const std::vector<const char*>& externalRequestedExtensions)
	{
		PRINT_DEBUG("Setting up Vulkan.");

		VkApplicationInfo applicationInfo  = vkInitStruct();
		applicationInfo.apiVersion         = gTargetVulkanVersion;
		applicationInfo.pApplicationName   = appName.empty() ? "Default App Name" : appName.data();
		applicationInfo.applicationVersion = gApplicationVersion;
		applicationInfo.pEngineName        = "Lustra Engine";
		applicationInfo.engineVersion      = gEngineVersion;

		SetupInstance(applicationInfo, externalRequestedExtensions);
		SetupDevice();
		SetupDebugMessenger();
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

		vkDestroyInstance(gVkInstance, gVkAllocationCallbacks);
	}

	void SetupInstance(
	    const VkApplicationInfo& vkApplicationInfo, const std::vector<const char*>& externalRequestedExtensions
	)
	{
		// NOTE: This is allowed to contain duplicate strings of the same extension.
		std::vector<const char*> instanceExtensions = {"VK_KHR_surface", "VK_KHR_win32_surface"};
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

		VkPhysicalDevice suitablePhysicalDevice                     = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties suitablePhysicalDeviceProperties = {};
		for (VkPhysicalDevice physicalDevice : physicalDevices)
		{
			VkPhysicalDeviceFeatures features     = {};
			VkPhysicalDeviceProperties properties = {};

			vkGetPhysicalDeviceProperties(physicalDevice, &properties);
			vkGetPhysicalDeviceFeatures(physicalDevice, &features);

			::PrintPhysicalDeviceProperties(properties);

			// Only check for suitable device if none has been found yet.
			if (suitablePhysicalDevice == VK_NULL_HANDLE)
			{
				bool isSuitableDevice = true;

				isSuitableDevice &= (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

				if (isSuitableDevice)
				{
					suitablePhysicalDevice           = physicalDevice;
					suitablePhysicalDeviceProperties = properties;
				}
			}
		}

		ENSURE_EX(suitablePhysicalDevice != VK_NULL_HANDLE, "Could not find a suitable GPU.");

		PRINT_LOG("Chosen physical device: '{}'", suitablePhysicalDeviceProperties.deviceName);
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
} // namespace Graphics
