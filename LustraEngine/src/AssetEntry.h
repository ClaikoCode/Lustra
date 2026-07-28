#pragma once

#include "MetadataPointer.h"

#include <cstdint>
#include <filesystem>

enum class AssetType : uint8_t
{
	Unknown = 0,
	Shader,
	Model,
	Texture,
	Material
};

// An entry into the asset database.
// Should only hold the information enough to load as a runtime resource and put into resource pools.
struct AssetEntry
{
	AssetType assetType = AssetType::Unknown;
	std::filesystem::path assetPath;
	MetadataPtr assetMetadata; // Metadata defined from the asset type
};
