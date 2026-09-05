#pragma once

#include "AssetManager.h"

#include <filesystem>

using PipelineCreationCallback = void (*)();

namespace RealtimeShaderCompiler
{
	// Make sure that pipelines are not in use before calling this.
	void RecompileShadersAndUpdatePipelineObjects(const std::unordered_set<std::filesystem::path>& paths);

	void RegisterPipelineWithShaders(PipelineCreationCallback callback, const std::vector<AssetID>& shaderAssets);
} // namespace RealtimeShaderCompiler
