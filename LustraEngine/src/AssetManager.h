#pragma once

#include "AssetEntry.h"
#include "LustraLib/Logger.h"
#include "LustraVulkan.h"
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

   All assets are potential resources but not all resources are guaranteed to come from assets.

*/

// Universally Unique Identifier
using UUID    = uint64_t;
using AssetID = UUID;

// Want implicit casting to UUID.
enum AssetKey : UUID
{
	AssetKeyUnknown           = 0,
	AssetKeyShaderVSTest      = 14552202811960402000u,
	AssetKeyShaderFSTest      = 8105587591145421000u,
	AssetKeyModelTest         = 6005506655734169000u,
	AssetKeyShaderVSModelTest = 11576320714956073000u,
	AssetKeyShaderFSModelTest = 15866899531044698000u,
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

	struct Model
	{
		bool useSkeleton = false;
		bool useMats     = false;
	};
} // namespace Metadata

class AssetDatabase
{
  public:
	void AddEntry(AssetID id, AssetType assetType, std::filesystem::path assetPath, MetadataPtr&& metaptr)
	{
		if (db.contains(id))
		{
			PRINT_WARNING("Overwriting asset with '{}'.", id);
		}

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

	void AddModel(AssetID modelID, std::filesystem::path modelPath, Metadata::Model&& modelMetadata)
	{
		AddEntry(
		    modelID,
		    AssetType::Model,
		    std::move(modelPath),
		    make_metadata_ptr<Metadata::Model>(std::move(modelMetadata))
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

	const AssetEntry& GetEntry(AssetID id);
}; // namespace AssetManager
