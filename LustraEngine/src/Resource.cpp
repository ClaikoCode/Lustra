#include "Resource.h"

#include "LustraLib/Logger.h"
#include "Model.h"
#include "Shader.h"
#include "Texture.h"

namespace Resource
{
	void ClearPoolsGPUMemory()
	{
		PRINT_LOG("Destroying GPU memory reciding in resource pools...");

		PoolInstance<Shader>().DestroyObjectsGPUMemory(&DestroyShader);
		PoolInstance<Material>().DestroyObjectsGPUMemory(&DestroyMaterial);
		PoolInstance<DepthTexture>().DestroyObjectsGPUMemory(&DestroyDepthTexture);
		PoolInstance<Texture2D>().DestroyObjectsGPUMemory(&DestroyTexture2D);
		PoolInstance<Model>().DestroyObjectsGPUMemory(&DestroyModel);

		PRINT_LOG("Done.");
	}
} // namespace Resource
