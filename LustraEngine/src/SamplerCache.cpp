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

	std::array<std::string_view, DefaultSamplerCount> samplerNames;

	samplerNames[DefaultSamplerLinearRepeat] = "Linear Repeat Sampler";
	samplerNames[DefaultSamplerLinearClamp]  = "Linear Clamp Sampler";
	samplerNames[DefaultSamplerPointRepeat]  = "Point Repeat Sampler";
	samplerNames[DefaultSamplerPointClamp]   = "Point Clamp Sampler";

	for (uint32_t i = 0; i < defaultSamplers.size(); i++)
	{
		RegisterSampler(samplerNames[i], defaultSamplers[i]);
	}
}

Handle<Resource::Sampler2D> SamplerCache::GetOrCreateSampler2D(const Resource::SamplerDesc2D& samplerDesc2D)
{
	const SamplerKey key = ::CreateSampler2DKey(samplerDesc2D);

	if (!m_samplerMap.contains(key))
	{
		RegisterSampler("Runtime Created Sampler", samplerDesc2D);
	}

	return m_samplerMap.at(key);
}

Handle<Resource::Sampler2D> SamplerCache::GetDefaultSampler(DefaultSampler defaultSampler)
{
	return GetOrCreateSampler2D(defaultSamplers[defaultSampler]);
}

// TODO: This implementation feels a bit backwards and should probably be revisited.
void SamplerCache::RegisterSampler(std::string_view name, const Resource::SamplerDesc2D& samplerDesc2D)
{
	Handle<Resource::Sampler2D> sampler2DHandle = Resource::Allocate<Resource::Sampler2D>();

	Resource::CreateSampler2D(name, sampler2DHandle, samplerDesc2D);

	m_samplerMap.emplace(::CreateSampler2DKey(samplerDesc2D), sampler2DHandle);
}

void SamplerCache::Destroy()
{
	for (auto [_, handle] : m_samplerMap)
	{
		Resource::Release(handle);
	}
}
