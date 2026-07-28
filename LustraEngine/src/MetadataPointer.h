#pragma once

#include <memory>

using MetadataPtr = std::unique_ptr<void, void (*)(void*)>;

template <typename T, typename... Args>
// By default, unique ptr doesn't store the underlying type when constructed so it cant create a void version.
// This function solves this by producing a unique ptr that has information on how to delete the object it stores.
MetadataPtr make_metadata_ptr(Args&&... args)
{
	return MetadataPtr(new T(std::forward<Args>(args)...), [](void* p) { delete static_cast<T*>(p); });
}
