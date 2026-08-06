#include "Resource.h"

#include "LustraLib/Logger.h"
#include "Model.h"
#include "Sampler.h"
#include "Shader.h"
#include "Texture.h"

namespace Resource
{
	void ClearPoolsGPUMemory()
	{
		PRINT_LOG("Destroying GPU memory reciding in resource pools...");

		PoolInstance<Shader>().DestroyObjectsGPUMemory(&DestroyShader);
		PoolInstance<Material>().DestroyObjectsGPUMemory(&DestroyMaterial);
		PoolInstance<Texture2D>().DestroyObjectsGPUMemory(&DestroyTexture2D);
		PoolInstance<Model>().DestroyObjectsGPUMemory(&DestroyModel);
		PoolInstance<Sampler2D>().DestroyObjectsGPUMemory(&DestroySampler2D);

		PRINT_LOG("Done.");
	}
} // namespace Resource
