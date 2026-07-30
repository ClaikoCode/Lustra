#include "Material.h"

#include "Resource.h"

namespace Resource
{
	void CreateMaterial(Handle<Material> materialHandle, const Material::Properties& props, const Material::Maps& maps)
	{
		Material& mat = GetRef(materialHandle);

		mat.properties = props;
		mat.maps       = maps;
	}

	void DestroyMaterial(Handle<Material> materialHandle)
	{
		Material& mat = GetRef(materialHandle);

		Resource::Release(mat.maps.albedo);
		Resource::Release(mat.maps.emissive);
		Resource::Release(mat.maps.normal);
		Resource::Release(mat.maps.orm);
	}
} // namespace Resource
