#include "RealtimeShaderCompiler.h"

#include "AssetRegistry.h"
#include "Graphics.h"
#include "GraphicsUtils.h"
#include "Resource.h"
#include "Shader.h"
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

	// Will take the input paths (can be both include and source files) and ensure that the output is only valid source
	// paths.
	std::unordered_set<std::filesystem::path> GetValidSourcePaths(
	    const std::unordered_set<std::filesystem::path>& paths
	)
	{
		std::unordered_set<std::filesystem::path> validPaths;

		// Fill with only source files.
		for (const auto& path : paths)
		{
			if (includeMap.contains(path))
			{
				validPaths.insert_range(includeMap.at(path));
			}
			else
			{
				validPaths.insert(path);
			}
		}

		// Remove all source files that have not been registered.
		std::erase_if(
		    validPaths,
		    [](const std::filesystem::path& path)
		{
			if (!shaderMap.contains(path))
			{
				PRINT_DEBUG(
				    "'{}' is not a registered shader source and wont be attempted for compilation.", path.string()
				);
				return true;
			}

			return false;
		}
		);

		return validPaths;
	}

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
} // namespace

namespace RealtimeShaderCompiler
{
	void RecompileShadersAndUpdatePipelineObjects(const std::unordered_set<std::filesystem::path>& paths)
	{
		PRINT_DEBUG("Recompiling all shaders and calling pipeline object creation callbacks.");

		std::unordered_set<std::filesystem::path> validSourcePaths = ::GetValidSourcePaths(paths);

		uint32_t failedRecompiles = 0u;
		for (const auto& path : validSourcePaths)
		{
			Handle<Resource::Shader> shaderHandle = shaderMap.at(path);

			Resource::Shader& shader = Resource::GetRef(shaderHandle);

			const std::unordered_set<std::string> oldIncludes = shader.artifact.includeFiles;

			bool success = ShaderCompilation::CompileShader(shader.compInfo, {}, shader.artifact);

			if (!success)
			{
				failedRecompiles++;
				continue;
			}

			const std::unordered_set<std::string>& newIncludes = shader.artifact.includeFiles;

			// Update includes if changed after compilation.
			if (oldIncludes != newIncludes)
			{
				for (const auto& oldInclude : oldIncludes)
				{
					auto includeIt = includeMap.find(oldInclude);
					if (includeIt == includeMap.end())
					{
						// This should never happen because if it existed as an old include at registration time then it
						// must be present in the include map. If this does happen, there is a bug somewhere.
						CHECK_UNREACHABLE();
						continue;
					}

					includeIt->second.erase(path);
					if (includeIt->second.empty())
					{
						includeMap.erase(oldInclude);
					}
				}

				for (const auto& newInclude : newIncludes)
				{
					includeMap[newInclude].insert(path);
				}
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
			    "{} shader{} failed to re-compile. Skipping re-creation of pipeline objects.",
			    failedRecompiles,
			    failedRecompiles > 1u ? "s" : ""
			);

			return;
		}

		std::unordered_set<PipelineCreationCallback> uniqueCallbacks;

		for (const auto& path : validSourcePaths)
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
		ENSURE(callback != nullptr);

		for (AssetID assetID : shaderAssets)
		{
			const AssetEntry& assetEntry = AssetManager::GetEntry(assetID);

			ENSURE(assetEntry.assetType == AssetType::Shader);

			const std::unordered_map<AssetID, Handle<Resource::Shader>>& shaderRegistry =
			    AssetRegistry::GetRegistry<Resource::Shader>();

			Handle<Resource::Shader> shaderHandle = shaderRegistry.at(assetID);

			::RegisterShader(shaderHandle);

			const Resource::Shader& shader = Resource::GetRef(shaderHandle);

			std::vector<PipelineCreationCallback>& callbacks = pipelineMap[shader.compInfo.shaderPath];

			if (!std::ranges::contains(callbacks, callback))
			{
				callbacks.push_back(callback);
			}
		}
	}
} // namespace RealtimeShaderCompiler
