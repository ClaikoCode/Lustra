#pragma once
#include "LustraGLM.h"
#include "ResourceTag.h"
#include "Texture.h"

namespace Resource
{
	struct Material : ResourceTag
	{
		enum class MapType : uint8_t
		{
			Albedo,
			Normal,
			Emissive,
			ORM // Occlusion Roughness Metallic
		};

		struct Properties
		{
			alignas(16) glm::vec4 albedoFactor   = {1.0f, 1.0f, 1.0f, 1.0f}; // Fully opaque
			alignas(16) glm::vec3 emissiveFactor = {0.0f, 0.0f, 0.0f};       // No emission
			float emissiveStrength               = {1.0f};                   // Identity scaling
			float occlusionStrength              = {1.0f};                   // Full AO applied
			float metallicFactor                 = {1.0f};                   // Fully metallic
			float roughnessFactor                = {1.0f};                   // Fully rough
			float _padding                       = {0.0f};
		} properties;

		struct Maps
		{
			Handle<Texture2D> albedo;
			Handle<Texture2D> normal;
			Handle<Texture2D> emissive;
			Handle<Texture2D> orm; // Occlusion (R), roughness (G), metallic (B).
		} maps;
	};

	void CreateMaterial(Handle<Material> materialHandle, const Material::Properties& props, const Material::Maps& maps);

	void DestroyMaterial(Handle<Material> materialHandle);
} // namespace Resource
