#pragma once

#include "Handle.h"
#include "Sampler.h"

#include <unordered_map>

using SamplerKey = uint64_t;

using SamplerMap = std::unordered_map<SamplerKey, Handle<Resource::Sampler2D>>;

enum DefaultSampler : uint8_t
{
	DefaultSamplerLinearRepeat,
	DefaultSamplerLinearClamp,
	DefaultSamplerPointRepeat,
	DefaultSamplerPointClamp,

	DefaultSamplerCount // Keep last.
};

namespace SamplerCache
{
	// Fills the sampler cache with default samplers.
	void Initialize();

	Handle<Resource::Sampler2D> GetOrCreateSampler2D(const Resource::SamplerDesc2D& samplerDesc2D);

	void Destroy();

	Handle<Resource::Sampler2D> GetDefaultSampler(DefaultSampler defaultSampler);

	inline std::array<Resource::SamplerDesc2D, DefaultSamplerCount> gDefaultSamplers = {};

}; // namespace SamplerCache
