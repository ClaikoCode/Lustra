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

class SamplerCache
{
  public:
	// Fills the sampler cache with default samplers.
	void Initialize();

	Handle<Resource::Sampler2D> GetOrCreateSampler2D(const Resource::SamplerDesc2D& samplerDesc2D);

	void Destroy();

  private:
	SamplerMap m_samplerMap;
};
