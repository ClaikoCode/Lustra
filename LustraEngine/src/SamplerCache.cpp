#include "SamplerCache.h"

#include "LustraLib/Utils.h"
#include "Resource.h"

#include <array>

namespace
{
	SamplerMap samplerMap;

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

	// TODO: This implementation feels a bit backwards and should probably be revisited.
	void RegisterSampler(std::string_view name, const Resource::SamplerDesc2D& samplerDesc2D)
	{
		Handle<Resource::Sampler2D> sampler2DHandle = Resource::Allocate<Resource::Sampler2D>();
		Resource::CreateSampler2D(name, sampler2DHandle, samplerDesc2D);

		::samplerMap.emplace(::CreateSampler2DKey(samplerDesc2D), sampler2DHandle);
	}
} // namespace

namespace SamplerCache
{
	void Initialize()
	{
		gDefaultSamplers[DefaultSamplerLinearRepeat] = {
		    .magFilter      = vk::Filter::eLinear,
		    .minFilter      = vk::Filter::eLinear,
		    .addressModeU   = vk::SamplerAddressMode::eRepeat,
		    .addressModeV   = vk::SamplerAddressMode::eRepeat,
		    .usesMipmapMode = true,
		};

		gDefaultSamplers[DefaultSamplerLinearClamp] = {
		    .magFilter      = vk::Filter::eLinear,
		    .minFilter      = vk::Filter::eLinear,
		    .addressModeU   = vk::SamplerAddressMode::eClampToEdge,
		    .addressModeV   = vk::SamplerAddressMode::eClampToEdge,
		    .usesMipmapMode = true,
		};

		gDefaultSamplers[DefaultSamplerPointRepeat] = {
		    .magFilter      = vk::Filter::eNearest,
		    .minFilter      = vk::Filter::eNearest,
		    .addressModeU   = vk::SamplerAddressMode::eRepeat,
		    .addressModeV   = vk::SamplerAddressMode::eRepeat,
		    .usesMipmapMode = false,
		};

		gDefaultSamplers[DefaultSamplerPointClamp] = {
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

		for (uint32_t i = 0; i < gDefaultSamplers.size(); i++)
		{
			::RegisterSampler(samplerNames[i], gDefaultSamplers[i]);
		}
	}

	Handle<Resource::Sampler2D> GetOrCreateSampler2D(const Resource::SamplerDesc2D& samplerDesc2D)
	{
		const SamplerKey key = ::CreateSampler2DKey(samplerDesc2D);

		if (!::samplerMap.contains(key))
		{
			::RegisterSampler("Runtime Created Sampler", samplerDesc2D);
		}

		return ::samplerMap.at(key);
	}

	Handle<Resource::Sampler2D> GetDefaultSampler(DefaultSampler defaultSampler)
	{
		return GetOrCreateSampler2D(gDefaultSamplers[defaultSampler]);
	}

	void Destroy()
	{
		for (auto [_, handle] : ::samplerMap)
		{
			Resource::Release(handle);
		}
	}
} // namespace SamplerCache
