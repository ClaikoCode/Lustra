#include "Resource.h"

#include "LustraLib/Logger.h"
#include "Shader.h"
#include "Texture.h"

namespace Resource
{
	void ClearPoolsGPUMemory()
	{
		PRINT_LOG("Destroying GPU memory reciding in resource pools...");

		PoolInstance<Shader>().DestroyObjectsGPUMemory(&DestroyShader);
		PoolInstance<DepthTexture>().DestroyObjectsGPUMemory(&DestroyDepthTexture);

		PRINT_LOG("Done.");
	}
} // namespace Resource
