#pragma once

#include "Handle.h"
#include "LustraVulkan.h"
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
	    std::string_view name,
	    Handle<Shader> shaderHandle,
	    const ShaderCompilationInfo& compInfo,
	    ShaderCompiler compiler,
	    const std::vector<std::string>& includeDirs
	);

	void DestroyShader(Handle<Shader> shaderHandle);
} // namespace Resource
