#pragma once

#include "AssetEntry.h"
#include "LustraVulkan.h"
#include "ShaderCompilerShared.h"

#include <cstdint>
#include <filesystem>

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
	AssetKeySponza            = 18259996468618960000u,
	AssetKeyCarConcept        = 7425371629934734000u,
	AssetKeyDragon            = 6441717997686723000u,
	AssetKeyBunny             = 8261371597526841000u,
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
		bool useMats     = true;
	};
} // namespace Metadata

namespace AssetManager
{
	void Setup();
	void Destroy();

	void AddEntry(AssetID id, AssetType assetType, std::filesystem::path assetPath, MetadataPtr&& metaptr);
	void AddShader(AssetID shaderID, std::filesystem::path shaderPath, Metadata::Shader&& shaderMetadata);
	void AddModel(AssetID modelID, std::filesystem::path modelPath, Metadata::Model&& modelMetadata);

	// Guarantees that asset exists in the database. Crashes otherwise.
	const AssetEntry& GetEntry(AssetID assetID);

	template <typename Meta>
	// Helper function to cast to correct metadata type.
	const Meta& GetMetadata(const AssetEntry& assetEntry)
	{
		return *static_cast<const Meta*>(assetEntry.assetMetadata.get());
	}

	template <typename Meta>
	// Gets metadata directly from ID (intended for when fetching asset entry first is unecessary).
	const Meta& GetMetadataFromID(AssetID assetID)
	{
		const AssetEntry& assetEntry = GetEntry(assetID);
		return GetMetadata<Meta>(assetEntry);
	}

}; // namespace AssetManager
