#pragma once

#include "LustraVulkan.h"
#include "Resource.h"
#include "ShaderCompilerShared.h"

#include <string>
#include <vector>

namespace Resource
{
	struct Shader : ResourceTag
	{
		vk::ShaderModule module;
		ShaderArtifact artifact;
	};

	void CreateShader(
	    Handle<Shader> shaderHandle,
	    const ShaderCompilationInfo& compInfo,
	    ShaderCompiler compiler,
	    const std::vector<std::string>& includeDirs
	);

	void DestroyShader(Handle<Shader> shaderHandle);
} // namespace Resource
