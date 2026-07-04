#pragma once

#include "ShaderCompilerShared.h"
#include "dxc/dxcapi.h"

#include <string_view>

// Checks if shader type A is of shader type B. Shader type B can be a bitwise combination.
#define IS_OF_SHADER_TYPE(shaderTypeA, shaderTypeB) (((shaderTypeA) & (shaderTypeB)) == (shaderTypeA))

namespace ShaderCompilation::DXC
{
	void Init();

	// Returns if compilation succeded or not
	[[nodiscard]] bool CompileShader(
	    const ShaderCompilationInfo& compInfo,
	    const std::vector<std::string>& includeDirectories,
	    ShaderArtifact& outArtifact
	);
} // namespace ShaderCompilation::DXC
