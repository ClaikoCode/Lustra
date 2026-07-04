#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

enum ShaderCompiler : uint8_t
{
	ShaderCompilerUnknown = 0,
	ShaderCompilerDXC
};

enum ShaderModel : uint8_t // NOLINT(readability-enum-initial-value)
{
	ShaderModelUnknown = 0,
	ShaderModel6_1,
	ShaderModel6_3,

	ShaderModelLatest = ShaderModel6_3
};

// ID for a type of shader from the rendering pipeline.
enum ShaderType : uint8_t
{
	ShaderTypeUnknown = 0,
	ShaderTypeVS      = 1 << 0,
	ShaderTypeHS      = 1 << 1,
	ShaderTypeDS      = 1 << 2,
	ShaderTypeGS      = 1 << 3,
	ShaderTypeFS      = 1 << 4,
	ShaderTypeCS      = 1 << 5,

	// Keep this after all the other shader types.
	ShaderTypeLast = 1 << 7,

	// Common combinations of shader types.
	ShaderTypeVS_PS         = ShaderTypeVS | ShaderTypeFS,
	ShaderTypeVS_PS_CS      = ShaderTypeVS | ShaderTypeFS | ShaderTypeCS,
	ShaderTypeHS_DS_GS      = ShaderTypeHS | ShaderTypeDS | ShaderTypeGS,
	ShaderTypeHS_ALL_BUT_VS = ShaderTypeHS | ShaderTypeDS | ShaderTypeGS | ShaderTypeFS | ShaderTypeCS,
	ShaderTypeAll           = ShaderTypeVS | ShaderTypeHS | ShaderTypeDS | ShaderTypeGS | ShaderTypeFS | ShaderTypeCS,

	// All shader types allowed in a graphics pipeline.
	ShaderTypeGraphics = ShaderTypeVS | ShaderTypeHS | ShaderTypeDS | ShaderTypeGS | ShaderTypeFS,
};

static inline std::string ShaderTypeToString(ShaderType shaderType)
{
	switch (shaderType)
	{
		case ShaderTypeVS:
			return "Vertex Shader";
		case ShaderTypeHS:
			return "Hull Shader";
		case ShaderTypeDS:
			return "Domain Shader";
		case ShaderTypeGS:
			return "Geometry Shader";
		case ShaderTypeFS:
			return "Fragment Shader";
		case ShaderTypeCS:
			return "Compute Shader";
		default:
			return "Unknown Shader Type";
	}
}

struct ShaderCompilationInfo
{
	std::filesystem::path shaderPath;
	std::string entryPoint = "main";

	// Holds macro defines inserted into the shader.
	std::vector<std::string> defines;

	ShaderType shaderType;
	ShaderModel shaderModel = ShaderModelLatest;
};

struct ShaderArtifact
{
	std::vector<std::byte> spirvData;

	// Files that are included in the shader. Is overwritten every compilation.
	std::unordered_set<std::string> includeFiles;
};
