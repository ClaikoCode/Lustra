#pragma once

#include "ResourceTag.h"

// General interface that will have static specializations for creating runtime resources from imported assets.
template <ResourceType T>
struct AssetImporter;
