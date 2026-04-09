#include "Graphics.h"

#include "GraphicsUtils.h"
#include "LustraLib/LustraLib.h"

#include <vector>

namespace
{
	// static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	//     VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	//     VkDebugUtilsMessageTypeFlagsEXT messageType,
	//     const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	//     void* userData
	// )
	// {
	// 	int a = 0;
	// }
} // namespace

namespace Graphics
{
	void SetupVulkan(const std::string_view appName, const std::vector<const char*>& externalRequestedExtensions)
	{
		VkApplicationInfo applicationInfo  = vkInitStruct();
		applicationInfo.apiVersion         = gTargetVulkanVersion;
		applicationInfo.pApplicationName   = appName.size() != 0 ? appName.data() : "Default App Name";
		applicationInfo.applicationVersion = gApplicationVersion;
		applicationInfo.pEngineName        = "Lustra Engine";
		applicationInfo.engineVersion      = gEngineVersion;

		// NOTE: This is allowed to contain duplicate strings of the same extension.
		std::vector<const char*> instanceExtensions = {"VK_KHR_surface", "VK_KHR_win32_surface"};
		for (const char* externalExtension : externalRequestedExtensions)
		{
			instanceExtensions.push_back(externalExtension);
		}

		std::vector<const char*> requestedLayers = {};

#if defined(_DEBUG) || defined(_LUSTRA_FORCE_VULKAN_DEBUG_LAYERS)
		// Exposes an API specific for controlling debug utilities.
		instanceExtensions.push_back("VK_EXT_debug_utils");
		// Enables Vk functions to validate arguments against the spec.
		requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

		// Check that requested validation layers are supported by the Vulkan instance.
		{
			uint32_t layerCount = 0;
			ASSERT_VK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

			std::vector<VkLayerProperties> instanceLayerProperties(layerCount);
			ASSERT_VK(vkEnumerateInstanceLayerProperties(&layerCount, instanceLayerProperties.data()));

			for (auto requestedLayer : requestedLayers)
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

				if (requestedLayerIsSupported == false)
				{
					PRINT_ERROR("Requested layer '{}' was not found. Aborting.", requestedLayer);
					LUSTRA_ASSERT(false);
				}
			}
		}

		VkInstanceCreateInfo instanceCreateInfo    = vkInitStruct();
		instanceCreateInfo.pApplicationInfo        = &applicationInfo;
		instanceCreateInfo.enabledExtensionCount   = instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
		instanceCreateInfo.enabledLayerCount       = requestedLayers.size();
		instanceCreateInfo.ppEnabledLayerNames     = requestedLayers.data();

		// FIXME: Gives error when using VK_VERSION_1_3 in api version that gives back a VK_SUCCESS. Find a way to
		// capture this properly and halt the program.
		ASSERT_VK(vkCreateInstance(&instanceCreateInfo, nullptr, &gVkInstance));
	}

	void TearDownVulkan()
	{
		CHECK_UNREACHABLE();
	}
} // namespace Graphics
