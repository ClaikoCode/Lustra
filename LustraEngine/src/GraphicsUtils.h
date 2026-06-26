#pragma once

#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "LustraVulkan.h"

#include <source_location>
#include <vector>

// Use the template function below for all vulkan.hpp cases
// This should only be used with functions that return vk results like the C API (vma for example).
#define ASSERT_VK(vkResult)                                                                                            \
	do                                                                                                                 \
	{                                                                                                                  \
		const VkResult err = (vkResult);                                                                               \
		if (err < 0)                                                                                                   \
		{                                                                                                              \
			PRINT_ERROR("Detected Vulkan Error: {}", vk::to_string(static_cast<vk::Result>(err)));                     \
			LUSTRA_ASSERT(false);                                                                                      \
		}                                                                                                              \
	} while (false)

template <typename T>
T AssertVk(vk::ResultValue<T> resultValue, std::source_location loc = std::source_location::current())
{
	if (resultValue.result != vk::Result::eSuccess)
	{
		LustraLib::Print(
		    LustraLib::OutputLevelError,
		    loc.file_name(),
		    loc.function_name(),
		    loc.line(),
		    "Detected Vulkan Error: {}",
		    vk::to_string(resultValue.result)
		);

		LUSTRA_ASSERT(false);
	}

	return std::move(resultValue.value);
}

// =================================
// 	   Graphics Helper Functions
// =================================

namespace GraphicsUtils
{
	struct FeatureNamesInfo
	{
		const char* const* names;
		uint16_t count;
		uint16_t firstOffset;
	};

	FeatureNamesInfo GetFeatureNames(vk::StructureType structType);

	using FeatureChain = vk::StructureChain<
	    vk::PhysicalDeviceFeatures2,
	    vk::PhysicalDeviceVulkan11Features,
	    vk::PhysicalDeviceVulkan12Features,
	    vk::PhysicalDeviceVulkan13Features,
	    vk::PhysicalDeviceVulkan14Features>;
} // namespace GraphicsUtils
