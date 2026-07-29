#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

constexpr uint32_t cMaxMipCount     = 0u;
constexpr uint32_t cUnknownMipCount = ~0u;

enum class ComponentType : uint8_t
{
	Unknown = 0,
	U8,
	U16,
	F32
};

enum class ColorSpace : uint8_t
{
	Unknown = 0,
	sRGB,
	Linear,
	HDR // TODO: Define this better.
};

using ImageData = std::vector<std::byte>;

struct ImageDimensions
{
	uint32_t width    = 0;
	uint32_t height   = 0;
	uint32_t channels = 0;
};

struct TextureArtifact
{
	ImageData data              = {};
	ImageDimensions dims        = {};
	ComponentType componentType = ComponentType::Unknown;
	uint32_t mipCount           = cUnknownMipCount;
	ColorSpace colorSpace       = ColorSpace::Unknown;
};

std::optional<TextureArtifact> ImportTextureRaw(
    const std::span<std::byte> rawBytes, ColorSpace requestedColorSpace, uint32_t requestedMipCount = cMaxMipCount
);

// Reads file and calls raw bytes version of import.
std::optional<TextureArtifact> ImportTexture(
    const std::filesystem::path& imagePath, ColorSpace requestedColorSpace, uint32_t requestedMipCount = cMaxMipCount
);
