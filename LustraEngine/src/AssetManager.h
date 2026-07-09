#pragma once

#include "LustraVulkan.h"
#include "ShaderCompilerShared.h"

#include <cstdint>
#include <unordered_map>

// Universally Unique Identifier
using UUID = uint64_t;

// NOLINTNEXTLINE(performance-enum-size)
enum ShaderID : UUID
{
	ShaderIDUnknown = 0ull,
	ShaderIDVSTest,
	ShaderIDFSTest,
};

struct ShaderPackage
{
	ShaderCompilationInfo compInfo;
	ShaderArtifact artifact;

	vk::ShaderModule module;

	ShaderCompiler compiler;
};

namespace AssetManager
{
	void Setup();
	void Destroy();

	void RegisterShader(ShaderID id, std::string_view shaderPath, ShaderType shaderType, ShaderCompiler compiler);
	void CompileHLSL(ShaderID id);
	void CreateShaderModule(ShaderID id);

	// Will both compile all shaders and create shader modules for them.
	void BuildShadersFromDatabase();

	ShaderPackage& GetShaderPackage(ShaderID id);
	vk::ShaderModule GetShaderModule(ShaderID id);
	std::string_view GetShaderEntryPoint(ShaderID id);

}; // namespace AssetManager
