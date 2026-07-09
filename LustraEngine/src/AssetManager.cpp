#include "AssetManager.h"

#include "Graphics.h"
#include "GraphicsUtils.h"
#include "LustraLib/Assert.h"
#include "ShaderCompilerDXC.h"

#include <utility>

static std::unordered_map<ShaderID, ShaderPackage> sShaderPackages;

namespace
{
} // namespace

namespace AssetManager
{

	void Setup()
	{
		ShaderCompilation::DXC::Init();
	}

	void Destroy()
	{
		for (auto& [_, shaderPackage] : sShaderPackages)
		{
			Graphics::gVkDevice.destroy(shaderPackage.module, Graphics::gAllocationCallbacks);
		}
	}

	void RegisterShader(ShaderID id, std::filesystem::path shaderPath, ShaderType shaderType, ShaderCompiler compiler)
	{
		auto& shaderPackage = sShaderPackages[id];

		shaderPackage.compiler            = compiler;
		shaderPackage.compInfo.shaderPath = std::move(shaderPath);
		shaderPackage.compInfo.shaderType = shaderType;
	}

	void CompileHLSL(ShaderID id)
	{
		auto& shaderPackage = sShaderPackages[id];

		const std::vector<std::string> includeDirs = {};
		ENSURE(ShaderCompilation::DXC::CompileShader(shaderPackage.compInfo, includeDirs, shaderPackage.artifact));
	}

	void CreateShaderModule(ShaderID id)
	{
		ShaderPackage& shaderPackage = sShaderPackages.at(id);

		ENSURE(!shaderPackage.artifact.spirvData.empty());

		// Destroy previous module if it already exists.
		// This means that the caller has to guarantee that the shader module is not in use before re-creation.
		if (shaderPackage.module != nullptr)
		{
			Graphics::gVkDevice.destroyShaderModule(shaderPackage.module, Graphics::gAllocationCallbacks);
		}

		const vk::ShaderModuleCreateInfo shaderModuleInfo = {
		    .codeSize = shaderPackage.artifact.spirvData.size(),
		    .pCode    = reinterpret_cast<const uint32_t*>(shaderPackage.artifact.spirvData.data()),
		};

		shaderPackage.module =
		    AssertVk(Graphics::gVkDevice.createShaderModule(shaderModuleInfo, Graphics::gAllocationCallbacks));
	}

	void BuildShadersFromDatabase()
	{
		for (auto& [id, _] : sShaderPackages)
		{
			CompileHLSL(id);
			CreateShaderModule(id);
		}
	}

	ShaderPackage& GetShaderPackage(ShaderID id)
	{
		auto iterator = sShaderPackages.find(id);
		ENSURE_EX(iterator != sShaderPackages.end(), "Shader '{}' has not been registered yet.", static_cast<UUID>(id));

		return iterator->second;
	}

	vk::ShaderModule GetShaderModule(ShaderID id)
	{
		const ShaderPackage& shaderPackage = GetShaderPackage(id);

		ENSURE_EX(
		    shaderPackage.module != VK_NULL_HANDLE,
		    "Shader module for '{}' has not been created yet.",
		    shaderPackage.compInfo.shaderPath.string()
		);

		return shaderPackage.module;
	}

	std::string_view GetShaderEntryPoint(ShaderID id)
	{
		const ShaderPackage& shaderPackage = GetShaderPackage(id);

		return shaderPackage.compInfo.entryPoint;
	}
} // namespace AssetManager
