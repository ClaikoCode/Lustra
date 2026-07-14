#pragma once

#include "AssetImporter.h"
#include "AssetManager.h"
#include "LustraLib/Logger.h"
#include "LustraLib/Utils.h"
#include "Resource.h"
#include "ResourceTag.h"

/*
    This API is made to bridge assets and their metadata contained in the asset database to their equivalent runtime
    resources used by engine internals. AssetIDs are stored in a map with asset handles as keys.

    That map is called the Registry.

    LOADING POLICY: Assets are lazy loaded if they do not exists in the map.
*/

namespace AssetRegistry
{
	template <ResourceType T>
	std::unordered_map<AssetID, Handle<T>>& GetRegistry()
	{
		static std::unordered_map<AssetID, Handle<T>> handleRegistry;

		return handleRegistry;
	}

	template <ResourceType T>
	void ClearRegistry()
	{
		std::unordered_map<AssetID, Handle<T>> reg = GetRegistry<T>();

		for (auto [_, handle] : reg)
		{
			handle.Release();
		}

		PRINT_DEBUG("Released '{}' registry.", Utils::TypeName<T>());
	}

	template <ResourceType T>
	[[nodiscard]] Handle<T> Resolve(AssetID id)
	{
		auto& registry = GetRegistry<T>();

		auto it = registry.find(id);

		// If never seen before, import the resource, add it to the registry, and give back the new handle.
		if (it == registry.end())
		{
			const AssetEntry& assetEntry = AssetManager::GetEntry(id);

			Handle<T> assetResourceHandle = AssetImporter<T>::Import(assetEntry);

			registry.emplace(id, assetResourceHandle);

			return assetResourceHandle;
		}
		else
		{
			return it->second;
		}
	}
} // namespace AssetRegistry
