#pragma once
#include <filesystem>
#include <string_view>

namespace Lustra::Paths
{
	inline constexpr std::string_view kCoreAssetRoot = LUSTRA_CORE_ASSET_ROOT;
	inline constexpr std::string_view kCoreShaderDir = LUSTRA_CORE_ASSET_ROOT "Shaders/";
	inline constexpr std::string_view kCoreHLSLDir   = LUSTRA_CORE_ASSET_ROOT "Shaders/hlsl/";
	inline constexpr std::string_view kCoreModelDir  = LUSTRA_CORE_ASSET_ROOT "Models/";

	// --- Helper functions to get std filesystem paths --

	inline std::filesystem::path ShaderDir()
	{
		return std::filesystem::path(kCoreShaderDir);
	}

	inline std::filesystem::path HLSLDir()
	{
		return std::filesystem::path(kCoreHLSLDir);
	}

	inline std::filesystem::path ModelDir()
	{
		return std::filesystem::path(kCoreModelDir);
	}

} // namespace Lustra::Paths
