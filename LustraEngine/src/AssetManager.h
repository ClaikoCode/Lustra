#pragma once

#include "LustraVulkan.h"
#include "Resource.h"
#include "ShaderCompilerShared.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

/*

    DEFINITIONS (that I'll hopefully follow as the project develops):

    An ASSET is referencing something on disk. It is an entity/item that is already defined offline by their metadata
   and can exist in the database.

    A RESOURCE is a broader definition for any defined set of data that the runtime engine makes use of. A resource can
   be created directly from an asset OR it can be defined during runtime.

   All assets are resources but not all resources are assets.

*/

// Universally Unique Identifier
using UUID    = uint64_t;
using AssetID = UUID;

// Want implicit casting to UUID.
enum AssetKey : UUID
{
	AssetKeyUnknown      = 0,
	AssetKeyShaderVSTest = 14552202811960402000u,
	AssetKeyShaderFSTest = 8105587591145421000u,
};

enum class AssetType : uint8_t
{
	Unknown = 0,
	Shader,
	Model,
	Texture,
	Material
};

namespace Metadata
{
	struct Shader
	{
		ShaderType shaderType   = ShaderTypeUnknown;
		ShaderCompiler compiler = ShaderCompiler::Unknown;

		// Holds macro defines inserted into the shader.
		std::vector<std::string> defines = {};

		ShaderModel shaderModel = ShaderModelLatest;
		std::string entryPoint  = "main";
	};
} // namespace Metadata

using MetadataPtr = std::unique_ptr<void, void (*)(void*)>;

template <typename T, typename... Args>
// By default, unique ptr doesnt store the underlying type when constructed so it cant create a void version.
// This function solves this by producing a unique ptr that has information on how to delete the object is stores.
MetadataPtr make_metadata_ptr(Args&&... args)
{
	return MetadataPtr(new T(std::forward<Args>(args)...), [](void* p) { delete static_cast<T*>(p); });
}

// Should only hold the information enough to load into resource pools
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

class AssetDatabase
{
  public:
	void AddEntry(AssetID id, AssetType assetType, std::filesystem::path assetPath, MetadataPtr&& metaptr)
	{
		db.emplace(
		    id,
		    AssetEntry{.assetType = assetType, .assetPath = std::move(assetPath), .assetMetadata = std::move(metaptr)}
		);
	}

	void AddShader(AssetID shaderID, std::filesystem::path shaderPath, Metadata::Shader&& shaderMetadata)
	{
		AddEntry(
		    shaderID,
		    AssetType::Shader,
		    std::move(shaderPath),
		    make_metadata_ptr<Metadata::Shader>(std::move(shaderMetadata))
		);
	}

	// Guarantees that asset exists in the database. Crashes otherwise.
	const AssetEntry& GetEntry(AssetID assetID)
	{
		return db.at(assetID);
	}

	const std::unordered_map<AssetID, AssetEntry>& GetDB()
	{
		return db;
	};

  private:
	std::unordered_map<AssetID, AssetEntry> db;
};

namespace AssetManager
{
	void Setup();
	void Destroy();

	// Because the compiled spirv and the shader module are so coupled, they are handled in the same function.
	// NOTE: Make sure to wait for GPU if shaders are re-compiled so module does not get overwritten whilst being used.
	void CompileAndBuildShader(AssetID id);

	// Will both compile all shaders and create shader modules for them.
	void BuildShadersFromDatabase();

	template <ResourceType T>
	std::unordered_map<AssetID, Handle<T>>& GetHandleRegistry()
	{
		static std::unordered_map<AssetID, Handle<T>> handleRegistry;

		return handleRegistry;
	}

	template <ResourceType T>
	// Get a handle given an ID, assuming it to exist in the database.
	inline Handle<T> GetHandle(AssetID id)
	{
		auto& handleRegistry = GetHandleRegistry<T>();

		auto it = handleRegistry.find(id);

		// If never seen before, allocate the resource, add it to the registry, and give back the new handle.
		if (it == handleRegistry.end())
		{
			ResourcePool<T>& poolInstance = Resource::PoolInstance<T>();

			Handle<T> handle   = poolInstance.Allocate();
			handleRegistry[id] = handle;

			return handle;
		}
		else
		{
			return it->second;
		}
	}

	const AssetEntry& GetEntry(AssetID id);
}; // namespace AssetManager
