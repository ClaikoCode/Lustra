#include "AssetManager.h"

#include "Graphics.h"
#include "GraphicsUtils.h"
#include "LustraLib/Assert.h"
#include "LustraPaths.h"
#include "Shader.h"
#include "ShaderCompilerDXC.h"

#include <unordered_map>
#include <utility>

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
		// Destroy shaders
		{
			auto& shaderRegistry = GetHandleRegistry<Resource::Shader>();

			for (auto& [id, shaderHandle] : shaderRegistry)
			{
				Resource::DestroyShader(shaderHandle);

				shaderHandle.Release();
			}
		}
	}

	void CompileAndBuildShader(AssetID id)
	{
		const AssetEntry& assetEntry     = GetEntry(id);
		Resource::Shader* shaderResource = GetHandle<Resource::Shader>(id).Get();
		ENSURE(shaderResource != nullptr);

		const auto& shaderMeta = assetEntry.GetMetadata<Metadata::Shader>();

		const ShaderCompilationInfo compInfo = {
		    .entryPoint  = shaderMeta.entryPoint,
		    .shaderType  = shaderMeta.shaderType,
		    .shaderModel = shaderMeta.shaderModel,
		    .shaderPath  = assetEntry.assetPath,
		    .defines     = {},
		};

		const std::vector<std::string> includeDirs = {};
		bool compSuccessful                        = false;
		switch (shaderMeta.compiler)
		{
			case ShaderCompiler::DXC:
				compSuccessful = ShaderCompilation::DXC::CompileShader(compInfo, includeDirs, shaderResource->artifact);
				break;

			default:
				PRINT_ERROR("Unknown shader compiler type.");
				CHECK_UNREACHABLE();
		}

		ENSURE(compSuccessful);

		// Destroy previous module if it already exists.
		// This means that the caller has to guarantee that the shader module is not in use before re-creation.
		if (shaderResource->module != nullptr)
		{
			Graphics::gVkDevice.destroyShaderModule(shaderResource->module, Graphics::gAllocationCallbacks);
		}

		const vk::ShaderModuleCreateInfo shaderModuleInfo = {
		    .codeSize = shaderResource->artifact.spirvData.size(),
		    .pCode    = reinterpret_cast<const uint32_t*>(shaderResource->artifact.spirvData.data()),
		};

		shaderResource->module =
		    AssertVk(Graphics::gVkDevice.createShaderModule(shaderModuleInfo, Graphics::gAllocationCallbacks));
	}

	void BuildShadersFromDatabase()
	{
		PRINT_DEBUG("Building shaders from database.");

		for (const auto& [id, assetEntry] : gAssetDatabase.GetDB())
		{
			if (assetEntry.assetType == AssetType::Shader)
			{
				CompileAndBuildShader(id);
			}
		}
	}

	const AssetEntry& GetEntry(AssetID id)
	{
		return gAssetDatabase.GetEntry(id);
	}
} // namespace AssetManager
