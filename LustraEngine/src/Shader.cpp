#include "Shader.h"

#include "Graphics.h"
#include "GraphicsUtils.h"
#include "Resource.h"
#include "ShaderCompilerDXC.h"

namespace Resource
{
	void CreateShader(
	    Handle<Shader> shaderHandle,
	    const ShaderCompilationInfo& compInfo,
	    ShaderCompiler compiler,
	    const std::vector<std::string>& includeDirs
	)
	{
		Shader* shader = shaderHandle.Get();
		ENSURE(shader != nullptr);

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
	}

	void DestroyShader(Handle<Shader> shaderHandle)
	{
		const Shader* shaderPtr = shaderHandle.Get();

		ENSURE(shaderPtr != nullptr);

		Graphics::gVkDevice.destroyShaderModule(shaderPtr->module);
	}
} // namespace Resource
