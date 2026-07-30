#pragma once

#include "ResourceTag.h"

#include <cstdint>

// Forward declaration
struct NullHandle;

template <ResourceType T>
struct Handle // Lightweight versioned resource handle
{
	static constexpr uint32_t kInvalidIndex = ~0u; // Sentinel value.

	uint32_t index      = kInvalidIndex;
	uint32_t generation = 0;

	bool operator==(const Handle&) const = default;
};

struct NullHandle
{
	template <ResourceType T>
	constexpr operator Handle<T>() const
	{
		return {}; // Nullhandle is defined to be a default initialized handle.
	}
};

inline constexpr NullHandle nullhandle;
