#pragma once

#include "AssetEntry.h"
#include "AssetImporter.h"
#include "Shader.h"

template <>
struct AssetImporter<Resource::Shader>
{
	static Handle<Resource::Shader> Import(const AssetEntry& assetEntry);
};
