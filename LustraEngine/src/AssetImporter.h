#pragma once

#include "ResourceTag.h"

// General interface that will have static specializations for creating runtime resources from imported assets.
// The struct has to implement a static Import function which takes in an AssetEntry and returns the handle to an
// allocated and instantiated resources.
template <ResourceType T>
struct AssetImporter;

// Example implementation:
/*
    template <>
    struct AssetImporter<Resource::Shader>
    {
        static Handle<Resource::Shader> Import(const AssetEntry& assetEntry);
    };
*/
