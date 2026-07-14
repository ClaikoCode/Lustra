#pragma once

#include "AssetImporter.h"
#include "AssetManager.h"
#include "Shader.h"

template <>
struct AssetImporter<Resource::Shader>
{
	static Handle<Resource::Shader> Import(const AssetEntry& assetEntry);
};
