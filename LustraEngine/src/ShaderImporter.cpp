#include "ShaderImporter.h"

#include "Resource.h"
#include "Shader.h"
#include "ShaderCompilerShared.h"

using Shader = Resource::Shader;

Handle<Shader> AssetImporter<Shader>::Import(const AssetEntry& assetEntry)
{
	Handle<Shader> shaderHandle = Resource::Allocate<Shader>();

	const auto& shaderMeta = assetEntry.GetMetadata<Metadata::Shader>();

	const ShaderCompilationInfo compInfo = {
	    .entryPoint  = shaderMeta.entryPoint,
	    .shaderType  = shaderMeta.shaderType,
	    .shaderModel = shaderMeta.shaderModel,
	    .shaderPath  = assetEntry.assetPath,
	    .defines     = {},
	};

	const std::vector<std::string> includeDirs = {};
	Resource::CreateShader(shaderHandle, compInfo, shaderMeta.compiler, includeDirs);

	return shaderHandle;
}
