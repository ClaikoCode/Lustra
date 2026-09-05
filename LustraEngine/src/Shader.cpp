#include "Shader.h"

#include "Graphics.h"
#include "GraphicsUtils.h"
#include "Resource.h"

namespace Resource
{
	void CreateShader(
	    std::string_view name,
	    Handle<Shader> shaderHandle,
	    const ShaderCompilationInfo& compInfo,
	    const std::vector<std::string>& includeDirs
	)
	{
		Shader* shader = Get(shaderHandle);
		ENSURE(shader != nullptr);
		ENSURE_EX(
		    shader->module == VK_NULL_HANDLE, "Shader must not already contain data (call delete before re-creating)."
		);

		shader->name = name;

		bool compSuccessful = ShaderCompilation::CompileShader(compInfo, includeDirs, shader->artifact);

		if (!compSuccessful)
		{
			PRINT_ERROR("Shader compilation failed without any SPIRV to fall back on.");
			CHECK_NOT_IMPL(); // TODO: Find if there is any way to have a "fallback shader".
			return;
		}

		const vk::ShaderModuleCreateInfo shaderModuleInfo = {
		    .codeSize = shader->artifact.spirvData.size(),
		    .pCode    = reinterpret_cast<const uint32_t*>(shader->artifact.spirvData.data()),
		};

		shader->module =
		    AssertVk(Graphics::gVkDevice.createShaderModule(shaderModuleInfo, Graphics::gAllocationCallbacks));

		shader->compInfo = compInfo;

		NameVk(Graphics::gVkDevice, shader->module, shader->name);
	}

	void DestroyShader(Handle<Shader> shaderHandle)
	{
		const Shader* shaderPtr = Get(shaderHandle);

		ENSURE(shaderPtr != nullptr);

		Graphics::gVkDevice.destroyShaderModule(shaderPtr->module);
	}
} // namespace Resource
