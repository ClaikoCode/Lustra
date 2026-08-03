#pragma once

#include "LustraGLM.h"
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
				LustraLib::Print(
				    LustraLib::OutputLevelDebug,
				    loc.file_name(),
				    loc.function_name(),
				    loc.line(),
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

inline vk::Result AssertVk(VkResult result, std::source_location loc = std::source_location::current())
{
	const auto castedResult = static_cast<vk::Result>(result);

	detail::AssertVkBase(castedResult, loc);
	return castedResult;
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

	// Converts milliseconds to nanoseconds.
	inline uint64_t TimeoutTimeMS(uint64_t timeInMilliseconds)
	{
		return timeInMilliseconds * 1'000'000u;
	}

	// Converts seconds to nanoseconds.
	inline uint64_t TimeoutTimeS(uint64_t timeInSeconds)
	{
		return timeInSeconds * 1'000'000'000u;
	}

	glm::u8vec4 SampleBilinearU8(
	    std::span<const std::byte> imageData, glm::vec2 uv, uint32_t width, uint32_t height, uint32_t channels
	);

	using FeatureChain = vk::StructureChain<
	    vk::PhysicalDeviceFeatures2,
	    vk::PhysicalDeviceVulkan11Features,
	    vk::PhysicalDeviceVulkan12Features,
	    vk::PhysicalDeviceVulkan13Features,
	    vk::PhysicalDeviceVulkan14Features>;
} // namespace GraphicsUtils
