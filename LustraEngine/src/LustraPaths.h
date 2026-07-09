#pragma once
#include <string_view>

namespace Lustra::Paths
{
	inline constexpr std::string_view kCoreAssetRoot = LUSTRA_CORE_ASSET_ROOT;
	inline constexpr std::string_view kCoreShaderDir = LUSTRA_CORE_ASSET_ROOT "Shaders/";
	inline constexpr std::string_view kCoreHLSLDir   = LUSTRA_CORE_ASSET_ROOT "Shaders/hlsl/";
	inline constexpr std::string_view kCoreModelDir  = LUSTRA_CORE_ASSET_ROOT "Models/";
} // namespace Lustra::Paths
