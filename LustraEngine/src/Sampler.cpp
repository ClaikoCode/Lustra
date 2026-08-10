#include "Sampler.h"

#include "Graphics.h"
#include "GraphicsUtils.h"
#include "Resource.h"

namespace Resource
{
	void CreateSampler2D(std::string_view name, Handle<Sampler2D> samplerHandle, const SamplerDesc2D& desc)
	{
		ENSURE(Get(samplerHandle) != nullptr);
		constexpr bool kUseMipmapLinear = true; // TODO: Make this a global setting for performance purposes.

		// Fill defaults.
		vk::SamplerCreateInfo samplerInfo = {
		    .flags                   = {},
		    .addressModeW            = vk::SamplerAddressMode::eRepeat, // Unused for 2D textures.
		    .mipLodBias              = 0.0f,
		    .anisotropyEnable        = vk::False, // TODO: Add global options for anisotropy.
		    .maxAnisotropy           = 1.0f,
		    .compareEnable           = vk::False,
		    .minLod                  = 0.0f,
		    .borderColor             = vk::BorderColor::eFloatTransparentBlack,
		    .unnormalizedCoordinates = vk::False,
		};

		// Fill arguments.
		samplerInfo.addressModeU = desc.addressModeU;
		samplerInfo.addressModeV = desc.addressModeV;
		samplerInfo.magFilter    = desc.magFilter;
		samplerInfo.minFilter    = desc.minFilter;
		samplerInfo.mipmapMode =
		    desc.usesMipmapMode && kUseMipmapLinear ? vk::SamplerMipmapMode::eLinear : vk::SamplerMipmapMode::eNearest;
		samplerInfo.maxLod = desc.usesMipmapMode ? vk::LodClampNone : 0.0f;

		Sampler2D& sampler = GetRef(samplerHandle);

		sampler.sampler = AssertVk(Graphics::gVkDevice.createSampler(samplerInfo, Graphics::gAllocationCallbacks));

		sampler.desc = desc;
		sampler.name = name;

		NameVk(Graphics::gVkDevice, sampler.sampler, sampler.name);
	}

	void DestroySampler2D(Handle<Sampler2D> samplerHandle)
	{
		ENSURE(Get(samplerHandle) != nullptr);

		Sampler2D& sampler = GetRef(samplerHandle);
		Graphics::gVkDevice.destroySampler(sampler.sampler);
	}
} // namespace Resource
