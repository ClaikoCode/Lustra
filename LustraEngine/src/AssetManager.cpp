#include "AssetManager.h"

#include "AssetRegistry.h"
#include "LustraPaths.h"
#include "Model.h"
#include "Shader.h"
#include "ShaderCompilerDXC.h"

using AssetDatabase = std::unordered_map<AssetID, AssetEntry>;

namespace
{
	AssetDatabase& GetDB()
	{
		static AssetDatabase dbInstance;

		return dbInstance;
	};
} // namespace

namespace AssetManager
{
	void Setup()
	{
		ShaderCompilation::DXC::Init();

		// Shaders
		{
			AddShader(
			    AssetKeyShaderFSTest,
			    Lustra::Paths::HLSLDir() / "FSTest.hlsl",
			    Metadata::Shader{.shaderType = ShaderTypeFS, .compiler = ShaderCompiler::DXC}
			);

			AddShader(
			    AssetKeyShaderVSTest,
			    Lustra::Paths::HLSLDir() / "VSTest.hlsl",
			    Metadata::Shader{.shaderType = ShaderTypeVS, .compiler = ShaderCompiler::DXC}
			);

			AddShader(
			    AssetKeyShaderFSModelTest,
			    Lustra::Paths::HLSLDir() / "FSModelTest.hlsl",
			    Metadata::Shader{.shaderType = ShaderTypeFS, .compiler = ShaderCompiler::DXC}
			);

			AddShader(
			    AssetKeyShaderVSModelTest,
			    Lustra::Paths::HLSLDir() / "VSModelTest.hlsl",
			    Metadata::Shader{.shaderType = ShaderTypeVS, .compiler = ShaderCompiler::DXC}
			);
		}

		// Models
		{
			AddModel(
			    AssetKeyModelTest,
			    // Lustra::Paths::ModelDir() / "Sponza/Sponza.gltf",
			    Lustra::Paths::ModelDir() / "CarConcept.glb",
			    Metadata::Model{
			        .useSkeleton = false,
			        .useMats     = false,
			    }
			);
		}
	}

	void Destroy()
	{
		PRINT_LOG("Destroying asset manager...");

		// Destroy registries
		{
			AssetRegistry::ClearRegistry<Resource::Shader>();
			AssetRegistry::ClearRegistry<Resource::Model>();
		}

		PRINT_LOG("Done.");
	}

	void AddEntry(AssetID id, AssetType assetType, std::filesystem::path assetPath, MetadataPtr&& metaptr)
	{
		if (::GetDB().contains(id))
		{
			PRINT_WARNING("Overwriting asset with '{}'.", id);
		}

		::GetDB().emplace(
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

	const AssetEntry& GetEntry(AssetID assetID)
	{
		return ::GetDB().at(assetID);
	}

} // namespace AssetManager
