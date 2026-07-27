#pragma once

#include "AssetEntry.h"
#include "AssetImporter.h"
#include "Handle.h"
#include "Model.h"

template <>
struct AssetImporter<Resource::Model>
{
	static Handle<Resource::Model> Import(const AssetEntry& modelEntry);
};
