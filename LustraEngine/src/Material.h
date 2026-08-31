#pragma once
#include "LustraGLM.h"
#include "ResourceTag.h"
#include "Sampler.h"
#include "Texture.h"

namespace Resource
{
	struct MaterialProperties
	{
		alignas(16) glm::vec4 albedoFactor   = {1.0f, 1.0f, 1.0f, 1.0f}; // Fully opaque
		alignas(16) glm::vec3 emissiveFactor = {0.0f, 0.0f, 0.0f};       // No emission
		float emissiveStrength               = {1.0f};                   // Identity scaling
		float occlusionStrength              = {1.0f};                   // Full AO applied
		float metallicFactor                 = {1.0f};                   // Fully metallic
		float roughnessFactor                = {1.0f};                   // Fully rough
		float _padding                       = {0.0f};
	};

	struct Material : ResourceTag
	{
		enum class MapType : uint8_t
		{
			Albedo,
			Normal,
			Emissive,
			ORM // Occlusion Roughness Metallic
		};

		MaterialProperties properties;

		struct Maps
		{
			Handle<Texture2D> albedo;
			Handle<Texture2D> normal;
			Handle<Texture2D> emissive;
			Handle<Texture2D> orm; // Occlusion (R), roughness (G), metallic (B).
		} maps;

		struct MapSamplers
		{
			Handle<Sampler2D> albedoSampler;
			Handle<Sampler2D> normalSampler;
			Handle<Sampler2D> emissiveSampler;
			Handle<Sampler2D> ormSampler;
		} samplers;
	};

	struct GPUMaterial
	{
		MaterialProperties properties = {};

		uint32_t albedoIndex   = 0;
		uint32_t normalIndex   = 0;
		uint32_t emissiveIndex = 0;
		uint32_t ormIndex      = 0;

		uint32_t albedoSampIndex   = 0;
		uint32_t normalSampIndex   = 0;
		uint32_t emissiveSampIndex = 0;
		uint32_t ormSampIndex      = 0;
	};

	void CreateMaterial(
	    std::string_view name,
	    Handle<Material> materialHandle,
	    const MaterialProperties& props,
	    const Material::Maps& maps,
	    const Material::MapSamplers& samplers
	);

	void DestroyMaterial(Handle<Material> materialHandle);

	GPUMaterial FillGPUMaterialStruct(const Material& material);
} // namespace Resource
