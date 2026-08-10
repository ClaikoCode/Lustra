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

	Handle<Resource::Sampler2D> GetDefaultSampler(DefaultSampler defaultSampler);

	std::array<Resource::SamplerDesc2D, DefaultSamplerCount> defaultSamplers = {};

  private:
	void RegisterSampler(std::string_view name, const Resource::SamplerDesc2D& samplerDesc2D);

	SamplerMap m_samplerMap;
};
