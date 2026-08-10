#include "Shader.h"

#include "Graphics.h"
#include "GraphicsUtils.h"
#include "Resource.h"
#include "ShaderCompilerDXC.h"

namespace Resource
{
	void CreateShader(
	    std::string_view name,
	    Handle<Shader> shaderHandle,
	    const ShaderCompilationInfo& compInfo,
	    ShaderCompiler compiler,
	    const std::vector<std::string>& includeDirs
	)
	{
		Shader* shader = Get(shaderHandle);
		ENSURE(shader != nullptr);

		shader->name = name;

		bool compSuccessful = false;
		switch (compiler)
		{
			case ShaderCompiler::DXC:
				compSuccessful = ShaderCompilation::DXC::CompileShader(compInfo, includeDirs, shader->artifact);
				break;

			default:
				PRINT_ERROR("Unknown shader compiler type.");
				CHECK_UNREACHABLE();
		}

		ENSURE(compSuccessful);

		const vk::ShaderModuleCreateInfo shaderModuleInfo = {
		    .codeSize = shader->artifact.spirvData.size(),
		    .pCode    = reinterpret_cast<const uint32_t*>(shader->artifact.spirvData.data()),
		};

		shader->module =
		    AssertVk(Graphics::gVkDevice.createShaderModule(shaderModuleInfo, Graphics::gAllocationCallbacks));

		NameVk(Graphics::gVkDevice, shader->module, shader->name);
	}

	void DestroyShader(Handle<Shader> shaderHandle)
	{
		const Shader* shaderPtr = Get(shaderHandle);

		ENSURE(shaderPtr != nullptr);

		Graphics::gVkDevice.destroyShaderModule(shaderPtr->module);
	}
} // namespace Resource
