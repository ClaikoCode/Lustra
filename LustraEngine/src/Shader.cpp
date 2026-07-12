#include "Shader.h"

#include "Graphics.h"

namespace Resource
{
	void DestroyShader(Handle<Shader> shaderHandle)
	{
		const Shader* shaderPtr = shaderHandle.Get();

		ENSURE(shaderPtr != nullptr);

		Graphics::gVkDevice.destroyShaderModule(shaderPtr->module);
	}
} // namespace Resource
