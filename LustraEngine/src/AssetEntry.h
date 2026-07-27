#pragma once

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

using MetadataPtr = std::unique_ptr<void, void (*)(void*)>;

template <typename T, typename... Args>
// By default, unique ptr doesn't store the underlying type when constructed so it cant create a void version.
// This function solves this by producing a unique ptr that has information on how to delete the object it stores.
MetadataPtr make_metadata_ptr(Args&&... args)
{
	return MetadataPtr(new T(std::forward<Args>(args)...), [](void* p) { delete static_cast<T*>(p); });
}

// An entry into the asset database.
// Should only hold the information enough to load as a runtime resource and put into resource pools.
struct AssetEntry
{
	AssetType assetType = AssetType::Unknown;
	std::filesystem::path assetPath;
	MetadataPtr assetMetadata; // Metadata defined from the asset type

	template <typename Meta>
	// Helper function to cast to correct metadata type.
	const Meta& GetMetadata() const
	{
		return *static_cast<const Meta*>(assetMetadata.get());
	}
};
