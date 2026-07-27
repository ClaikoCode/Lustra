#include "AssetManager.h"

#include "AssetRegistry.h"
#include "LustraPaths.h"
#include "Model.h"
#include "Shader.h"
#include "ShaderCompilerDXC.h"

namespace
{
	AssetDatabase gAssetDatabase;
} // namespace

namespace AssetManager
{
	void Setup()
	{
		ShaderCompilation::DXC::Init();

		// Shaders
		{
			gAssetDatabase.AddShader(
			    AssetKeyShaderFSTest,
			    Lustra::Paths::HLSLDir() / "FSTest.hlsl",
			    Metadata::Shader{.shaderType = ShaderTypeFS, .compiler = ShaderCompiler::DXC}
			);

			gAssetDatabase.AddShader(
			    AssetKeyShaderVSTest,
			    Lustra::Paths::HLSLDir() / "VSTest.hlsl",
			    Metadata::Shader{.shaderType = ShaderTypeVS, .compiler = ShaderCompiler::DXC}
			);

			gAssetDatabase.AddShader(
			    AssetKeyShaderFSModelTest,
			    Lustra::Paths::HLSLDir() / "FSModelTest.hlsl",
			    Metadata::Shader{.shaderType = ShaderTypeFS, .compiler = ShaderCompiler::DXC}
			);

			gAssetDatabase.AddShader(
			    AssetKeyShaderVSModelTest,
			    Lustra::Paths::HLSLDir() / "VSModelTest.hlsl",
			    Metadata::Shader{.shaderType = ShaderTypeVS, .compiler = ShaderCompiler::DXC}
			);
		}

		// Models
		{
			gAssetDatabase.AddModel(
			    AssetKeyModelTest,
			    Lustra::Paths::ModelDir() / "DragonDispersion.glb",
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

	const AssetEntry& GetEntry(AssetID id)
	{
		return gAssetDatabase.GetEntry(id);
	}
} // namespace AssetManager
