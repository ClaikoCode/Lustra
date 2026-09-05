#pragma once

#include "ShaderCompilerShared.h"

// Checks if shader type A is of shader type B. Shader type B can be a bitwise combination.
#define IS_OF_SHADER_TYPE(shaderTypeA, shaderTypeB) (((shaderTypeA) & (shaderTypeB)) == (shaderTypeA))

namespace ShaderCompilation::DXC
{
	void Init();

	// Returns if compilation succeeded or not.
	// Artifact will only be modified if compilation was successful.
	[[nodiscard]] bool CompileShader(
	    const ShaderCompilationInfo& compInfo,
	    const std::vector<std::string>& includeDirectories,
	    ShaderArtifact& outArtifact
	);
} // namespace ShaderCompilation::DXC
