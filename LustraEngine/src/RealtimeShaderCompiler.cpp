#include "RealtimeShaderCompiler.h"

#include "AssetRegistry.h"
#include "Graphics.h"
#include "GraphicsUtils.h"
#include "Resource.h"
#include "ShaderCompilerShared.h"

#include <unordered_map>
#include <unordered_set>

namespace
{
	// Keys are include files and values are the source files they touch.
	using IncludeMap = std::unordered_map<std::filesystem::path, std::unordered_set<std::filesystem::path>>;

	using PipelineMap = std::unordered_map<std::filesystem::path, std::vector<PipelineCreationCallback>>;
	using ShaderMap   = std::unordered_map<std::filesystem::path, Handle<Resource::Shader>>;

	ShaderMap shaderMap;
	PipelineMap pipelineMap;

	IncludeMap includeMap;

	// Will take the input paths (can be both include and source files) and ensure that the output is only source paths.
	std::unordered_set<std::filesystem::path> GetUniqueSourcePaths(
	    const std::unordered_set<std::filesystem::path>& paths
	)
	{
		std::unordered_set<std::filesystem::path> uniquePaths;

		for (const auto& path : paths)
		{
			if (includeMap.contains(path))
			{
				uniquePaths.insert_range(includeMap.at(path));
			}
			else
			{
				uniquePaths.insert(path);
			}
		}

		return uniquePaths;
	}

} // namespace

namespace RealtimeShaderCompiler
{
	void RegisterShader(Handle<Resource::Shader> shaderHandle)
	{
		Resource::Shader shader                     = Resource::GetRef(shaderHandle);
		const std::filesystem::path& sourceFilePath = shader.compInfo.shaderPath;

		// Associate all included files with the source file.
		for (const auto& includePath : shader.artifact.includeFiles)
		{
			includeMap[includePath].insert(sourceFilePath);
		}

		shaderMap[Resource::GetRef(shaderHandle).compInfo.shaderPath] = shaderHandle;
	}

	void RecompileShadersAndUpdatePipelineObjects(const std::unordered_set<std::filesystem::path>& paths)
	{
		PRINT_DEBUG("Recompiling all shaders and calling pipeline object creation callbacks.");

		const std::unordered_set<std::filesystem::path> uniqueSourcePaths = ::GetUniqueSourcePaths(paths);

		uint32_t failedRecompiles = 0u;
		for (const auto& path : uniqueSourcePaths)
		{
			Handle<Resource::Shader> shaderHandle = shaderMap.at(path);

			Resource::Shader& shader = Resource::GetRef(shaderHandle);
			bool success             = ShaderCompilation::CompileShader(shader.compInfo, {}, shader.artifact);

			if (!success)
			{
				failedRecompiles++;
				continue;
			}

			Graphics::gVkDevice.destroyShaderModule(shader.module, Graphics::gAllocationCallbacks);

			const vk::ShaderModuleCreateInfo shaderModuleInfo = {
			    .codeSize = shader.artifact.spirvData.size(),
			    .pCode    = reinterpret_cast<const uint32_t*>(shader.artifact.spirvData.data()),
			};

			shader.module =
			    AssertVk(Graphics::gVkDevice.createShaderModule(shaderModuleInfo, Graphics::gAllocationCallbacks));
		}

		if (failedRecompiles > 0u)
		{
			PRINT_ERROR(
			    "{} {} failed to re-compile. Skipping re-creation of pipeline objects.",
			    failedRecompiles,
			    failedRecompiles == 1 ? "shader" : "shaders"
			);
			return;
		}

		std::unordered_set<PipelineCreationCallback> uniqueCallbacks;

		for (const auto& path : uniqueSourcePaths)
		{
			const std::vector<PipelineCreationCallback>& callbacks = pipelineMap.at(path);
			uniqueCallbacks.insert_range(callbacks);
		}

		// Order of calls does not matter.
		for (const auto& callback : uniqueCallbacks) // NOLINT(bugprone-nondeterministic-pointer-iteration-order)
		{
			callback();
		}

		PRINT_DEBUG("Done with recompilation and updating.");
	}

	void RegisterPipelineWithShaders(PipelineCreationCallback callback, const std::vector<AssetID>& shaderAssets)
	{
		for (AssetID assetID : shaderAssets)
		{
			const AssetEntry& assetEntry = AssetManager::GetEntry(assetID);

			ENSURE(assetEntry.assetType == AssetType::Shader);

			const std::unordered_map<AssetID, Handle<Resource::Shader>>& shaderRegistry =
			    AssetRegistry::GetRegistry<Resource::Shader>();

			Handle<Resource::Shader> shaderHandle = shaderRegistry.at(assetID);

			RegisterShader(shaderHandle);

			const Resource::Shader& shader = Resource::GetRef(shaderHandle);

			std::vector<PipelineCreationCallback>& callbacks = pipelineMap[shader.compInfo.shaderPath];

			if (!std::ranges::contains(callbacks, callback))
			{
				callbacks.push_back(callback);
			}
		}
	}
} // namespace RealtimeShaderCompiler
