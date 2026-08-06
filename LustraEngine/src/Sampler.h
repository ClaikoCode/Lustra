#pragma once

#include "Handle.h"
#include "LustraVulkan.h"
#include "ResourceTag.h"

namespace Resource
{
	struct SamplerDesc2D
	{
		vk::Filter magFilter                = vk::Filter::eLinear;
		vk::Filter minFilter                = vk::Filter::eLinear;
		vk::SamplerAddressMode addressModeU = vk::SamplerAddressMode::eRepeat;
		vk::SamplerAddressMode addressModeV = vk::SamplerAddressMode::eRepeat;
		bool usesMipmapMode                 = true; // Lustra determines how this is interpreted
	};

	struct Sampler2D : ResourceTag
	{
		SamplerDesc2D desc = {};

		vk::Sampler sampler = nullptr;
	};

	void CreateSampler2D(Handle<Sampler2D> samplerHandle, const SamplerDesc2D& desc);
	void DestroySampler2D(Handle<Sampler2D> samplerHandle);
} // namespace Resource
