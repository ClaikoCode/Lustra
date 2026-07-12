#pragma once

#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "LustraVulkan.h"

#include <source_location>

namespace detail
{
	// Dont use this directly.
	inline void AssertVkBase(vk::Result result, const std::source_location& loc)
	{
		// Negative VkResult = real error. Zero/positive = success or info code.
		if (static_cast<std::int32_t>(result) < 0)
		{
			LustraLib::Print(
			    LustraLib::OutputLevelError,
			    loc.file_name(),
			    loc.function_name(),
			    loc.line(),
			    "Detected Vulkan Error: {}",
			    vk::to_string(result)
			);

			LUSTRA_ASSERT(false);
		}
		else
		{
			if (result != vk::Result::eSuccess)
			{
				PRINT_DEBUG(
				    "Vk result '{}' is neither an error or success. Make sure to handle it corectly.",
				    vk::to_string(result)
				);
			}
		}
	}
} // namespace detail

template <typename T>
// Will return the value if assertion did not fail.
[[nodiscard]] T AssertVk(vk::ResultValue<T> resultValue, std::source_location loc = std::source_location::current())
{
	detail::AssertVkBase(resultValue.result, loc);
	return std::move(resultValue.value);
}

// Will return result if assertion did not fail. This works on Vulkan C API result values through implicit casting.
inline vk::Result AssertVk(vk::Result result, std::source_location loc = std::source_location::current())
{
	detail::AssertVkBase(result, loc);
	return result;
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
