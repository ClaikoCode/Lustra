#include "Material.h"

#include "Resource.h"

namespace Resource
{
	void CreateMaterial(Handle<Material> materialHandle, const MaterialProperties& props, const Material::Maps& maps)
	{
		Material& mat = GetRef(materialHandle);

		mat.properties = props;
		mat.maps       = maps;
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

		gpuMaterial.albedoIndex   = GetRef(material.maps.albedo).bindlessIndex;
		gpuMaterial.emissiveIndex = GetRef(material.maps.emissive).bindlessIndex;
		gpuMaterial.normalIndex   = GetRef(material.maps.normal).bindlessIndex;
		gpuMaterial.ormIndex      = GetRef(material.maps.orm).bindlessIndex;

		return gpuMaterial;
	}
} // namespace Resource
