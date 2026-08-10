#include "ShaderImporter.h"

#include "AssetManager.h"
#include "Resource.h"
#include "Shader.h"
#include "ShaderCompilerShared.h"

using Shader = Resource::Shader;

Handle<Shader> AssetImporter<Shader>::Import(const AssetEntry& assetEntry)
{
	Handle<Shader> shaderHandle = Resource::Allocate<Shader>();

	const auto& shaderMeta = AssetManager::GetMetadata<Metadata::Shader>(assetEntry);

	const ShaderCompilationInfo compInfo = {
	    .entryPoint  = shaderMeta.entryPoint,
	    .shaderType  = shaderMeta.shaderType,
	    .shaderModel = shaderMeta.shaderModel,
	    .shaderPath  = assetEntry.assetPath,
	    .defines     = {},
	};

	const std::vector<std::string> includeDirs = {};
	Resource::CreateShader(
	    compInfo.shaderPath.filename().c_str(), shaderHandle, compInfo, shaderMeta.compiler, includeDirs
	);

	return shaderHandle;
}
