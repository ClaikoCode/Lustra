#pragma once

#include "AssetManager.h"
#include "Shader.h"

#include <filesystem>

using PipelineCreationCallback = void (*)();

namespace RealtimeShaderCompiler
{
	void SyncWithShaderAssets();
	void RegisterShader(Handle<Resource::Shader> shaderHandle);

	// Will re-create the underlying Shader resource tied to a shader path.
	// NOTE: This does not affect any pipeline objects that use the shaders.
	void RecompileShader(const std::filesystem::path& shaderPath);
	void RecompileShaders(const std::unordered_set<std::filesystem::path>& sourcePaths);

	// Make sure that pipelines are not in use before calling this.
	void RecompileShadersAndUpdatePipelineObjects(const std::unordered_set<std::filesystem::path>& paths);

	void RegisterPipelineWithShaders(PipelineCreationCallback callback, const std::vector<AssetID>& shaderAssets);
} // namespace RealtimeShaderCompiler
