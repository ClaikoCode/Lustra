#include "AssetManager.h"

#include "AssetRegistry.h"
#include "LustraPaths.h"
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
	}

	void Destroy()
	{
		PRINT_LOG("Destroying asset manager...");

		// Destroy shaders
		{
			AssetRegistry::ClearRegistry<Resource::Shader>();
		}

		PRINT_LOG("Done.");
	}

	const AssetEntry& GetEntry(AssetID id)
	{
		return gAssetDatabase.GetEntry(id);
	}
} // namespace AssetManager
