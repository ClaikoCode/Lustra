#include "Material.h"

#include "Resource.h"

namespace Resource
{
	void CreateMaterial(
	    std::string_view name,
	    Handle<Material> materialHandle,
	    const MaterialProperties& props,
	    const Material::Maps& maps
	)
	{
		Material& mat = GetRef(materialHandle);

		mat.properties = props;
		mat.maps       = maps;
		mat.name       = std::string(name);
	}

	void DestroyMaterial(Handle<Material> materialHandle)
	{
		Material& mat = GetRef(materialHandle);

		Release(mat.maps.albedo);
		Release(mat.maps.emissive);
		Release(mat.maps.normal);
		Release(mat.maps.orm);
	}

	GPUMaterial FillGPUMaterialStruct(const Material& material)
	{
		GPUMaterial gpuMaterial = {};

		gpuMaterial.properties = material.properties;

		gpuMaterial.albedoIndex   = material.maps.albedo.index;
		gpuMaterial.emissiveIndex = material.maps.emissive.index;
		gpuMaterial.normalIndex   = material.maps.normal.index;
		gpuMaterial.ormIndex      = material.maps.orm.index;

		return gpuMaterial;
	}
} // namespace Resource
