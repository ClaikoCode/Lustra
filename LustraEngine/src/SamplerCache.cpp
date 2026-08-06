#include "SamplerCache.h"

#include "LustraLib/Utils.h"
#include "Resource.h"

#include <array>

namespace
{
	SamplerKey CreateSampler2DKey(const Resource::SamplerDesc2D& desc)
	{
		size_t key = 0ull;
		Utils::HashCombine(key, static_cast<size_t>(desc.addressModeU));
		Utils::HashCombine(key, static_cast<size_t>(desc.addressModeV));
		Utils::HashCombine(key, static_cast<size_t>(desc.minFilter));
		Utils::HashCombine(key, static_cast<size_t>(desc.magFilter));
		Utils::HashCombine(key, static_cast<size_t>(desc.usesMipmapMode));

		return key;
	}
} // namespace

void SamplerCache::Initialize()
{
	std::array<Resource::SamplerDesc2D, DefaultSamplerCount> defaultSamplers = {};

	defaultSamplers[DefaultSamplerLinearRepeat] = {
	    .magFilter      = vk::Filter::eLinear,
	    .minFilter      = vk::Filter::eLinear,
	    .addressModeU   = vk::SamplerAddressMode::eRepeat,
	    .addressModeV   = vk::SamplerAddressMode::eRepeat,
	    .usesMipmapMode = true,
	};

	defaultSamplers[DefaultSamplerLinearClamp] = {
	    .magFilter      = vk::Filter::eLinear,
	    .minFilter      = vk::Filter::eLinear,
	    .addressModeU   = vk::SamplerAddressMode::eClampToEdge,
	    .addressModeV   = vk::SamplerAddressMode::eClampToEdge,
	    .usesMipmapMode = true,
	};

	defaultSamplers[DefaultSamplerPointRepeat] = {
	    .magFilter      = vk::Filter::eNearest,
	    .minFilter      = vk::Filter::eNearest,
	    .addressModeU   = vk::SamplerAddressMode::eRepeat,
	    .addressModeV   = vk::SamplerAddressMode::eRepeat,
	    .usesMipmapMode = false,
	};

	defaultSamplers[DefaultSamplerPointClamp] = {
	    .magFilter      = vk::Filter::eNearest,
	    .minFilter      = vk::Filter::eNearest,
	    .addressModeU   = vk::SamplerAddressMode::eClampToEdge,
	    .addressModeV   = vk::SamplerAddressMode::eClampToEdge,
	    .usesMipmapMode = true,
	};

	// Assert that the default samplers are the first ones to be instantiated so indices line up correctly.
	VALIDATE(Resource::PoolInstance<Resource::Sampler2D>().GetIndicesOfAliveObjects().empty());

	for (const Resource::SamplerDesc2D& desc : defaultSamplers)
	{
		// Since there are no entries in the cache yet, this will always result in "create".
		GetOrCreateSampler2D(desc);
	}
}

Handle<Resource::Sampler2D> SamplerCache::GetOrCreateSampler2D(const Resource::SamplerDesc2D& samplerDesc2D)
{
	const SamplerKey key = ::CreateSampler2DKey(samplerDesc2D);

	if (!m_samplerMap.contains(key))
	{
		Handle<Resource::Sampler2D> sampler2DHandle = Resource::Allocate<Resource::Sampler2D>();

		Resource::CreateSampler2D(sampler2DHandle, samplerDesc2D);

		m_samplerMap.emplace(key, sampler2DHandle);
	}

	return m_samplerMap.at(key);
}

void SamplerCache::Destroy()
{
	for (auto [_, handle] : m_samplerMap)
	{
		Resource::Release(handle);
	}
}
