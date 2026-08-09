#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS   // Use ResultValue<T>
#define VULKAN_HPP_NO_CONSTRUCTORS // Use designated initializers

#include "vma/vk_mem_alloc.h"

#include <vulkan/vulkan.hpp>

// Lustra makes use of the dynamic dispatcher so make sure this is defined at compile time.
static_assert(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1, "Static dispatcher active in this TU");
