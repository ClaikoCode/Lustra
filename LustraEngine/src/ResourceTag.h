#pragma once

#include <concepts>

struct ResourceTag
{
	// This is a struct that all resources should inherit from.
	// Its purpose is to be able to identify if a type is a resource or not to differentiate them from other normal data
	// structures.
};

// Concept that is to be checked by anything that is tied to templated usage of resources.
template <typename T>
concept ResourceType = std::derived_from<T, ResourceTag>;
