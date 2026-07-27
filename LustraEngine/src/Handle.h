#pragma once

#include "ResourceTag.h"

#include <cstdint>

template <ResourceType T>
struct Handle // Lightweight versioned resource handle
{
	uint32_t index;
	uint32_t generation;

	// TODO: Make this a POD and move functions to Resource.h
	T* Get();
	const T* Get() const;
	void Release();
};
