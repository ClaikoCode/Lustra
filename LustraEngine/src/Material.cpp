#include "Material.h"

#include "Resource.h"

namespace Resource
{
	void CreateMaterial(
	    std::string_view name,
	    Handle<Material> materialHandle,
	    const MaterialProperties& props,
	    const Material::Maps& maps,
	    const Material::MapSamplers& samplers
	)
	{
		Material& mat = GetRef(materialHandle);

		mat.properties = props;
		mat.maps       = maps;
		mat.samplers   = samplers;
		mat.name       = std::string(name);
	}

	void DestroyMaterial(Handle<Material> materialHandle)
	{
		Material& mat = GetRef(materialHandle);

		Release(mat.maps.albedo);
		Release(mat.maps.emissive);
		Release(mat.maps.normal);
		Release(mat.maps.orm);

		Release(mat.samplers.albedoSampler);
		Release(mat.samplers.emissiveSampler);
		Release(mat.samplers.normalSampler);
		Release(mat.samplers.ormSampler);
	}

	GPUMaterial FillGPUMaterialStruct(const Material& material)
	{
		GPUMaterial gpuMaterial = {};

		gpuMaterial.properties = material.properties;

		gpuMaterial.albedoIndex   = material.maps.albedo.index;
		gpuMaterial.emissiveIndex = material.maps.emissive.index;
		gpuMaterial.normalIndex   = material.maps.normal.index;
		gpuMaterial.ormIndex      = material.maps.orm.index;

		gpuMaterial.albedoSampIndex   = material.samplers.albedoSampler.index;
		gpuMaterial.emissiveSampIndex = material.samplers.emissiveSampler.index;
		gpuMaterial.normalSampIndex   = material.samplers.normalSampler.index;
		gpuMaterial.ormSampIndex      = material.samplers.ormSampler.index;

		return gpuMaterial;
	}
} // namespace Resource
