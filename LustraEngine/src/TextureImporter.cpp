#include "TextureImporter.h"

#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "LustraLib/Utils.h"
#include "stb/stb_image.h"

enum class DecodeError : uint8_t
{
	InvalidChannelCount,
	FailedLoad
};

struct ImageLoadError
{
	DecodeError errCode;
	std::string info;
};

struct ImageDescSTBI
{
	ImageData data;
	ImageDimensions dims;
	ComponentType componentType;
};

using ReturnValueSTBI = std::expected<ImageDescSTBI, ImageLoadError>;

// Forces bytes to always load as RGBA to be best compatible with engine consumption.
// Packed array of [RGBARGBARGBA...] values.
// Supported formats: float, U16 bit, U8 bit.
ReturnValueSTBI DecodeRawImageBytesWithSTBI(const std::span<const std::byte> rawBytes)
{
	const stbi_uc* buffer = reinterpret_cast<const stbi_uc*>(rawBytes.data());
	const int len         = static_cast<int>(rawBytes.size_bytes());

	// Determine the component type first as this determines how to load the file and how to copy the output data from
	// stbi.
	ComponentType imageComponentType = ComponentType::Unknown;
	if (stbi_is_hdr_from_memory(buffer, len) != 0)
	{
		imageComponentType = ComponentType::F32;
	}
	else if (stbi_is_16_bit_from_memory(buffer, len) != 0)
	{
		imageComponentType = ComponentType::U16;
	}
	else
	{
		imageComponentType = ComponentType::U8;
	}

	int x;
	int y;
	int channels;
	uint8_t* decodedImagePtr = nullptr;
	switch (imageComponentType)
	{
		case ComponentType::F32:
			decodedImagePtr =
			    reinterpret_cast<uint8_t*>(stbi_loadf_from_memory(buffer, len, &x, &y, &channels, STBI_rgb_alpha));
			break;
		case ComponentType::U16:
			decodedImagePtr =
			    reinterpret_cast<uint8_t*>(stbi_load_16_from_memory(buffer, len, &x, &y, &channels, STBI_rgb_alpha));
			break;
		case ComponentType::U8:
			decodedImagePtr = stbi_load_from_memory(buffer, len, &x, &y, &channels, STBI_rgb_alpha);
			break;
		default:
			CHECK_UNREACHABLE();
	}

	if (decodedImagePtr == nullptr)
	{
		const char* reason = stbi_failure_reason();
		return std::unexpected(
		    ImageLoadError{
		        .errCode = DecodeError::FailedLoad,
		        .info    = (reason != nullptr) ? reason : "unknown",
		    }
		);
	}

	if (channels < 1 || channels > 4)
	{
		return std::unexpected(
		    ImageLoadError{
		        .errCode = DecodeError::InvalidChannelCount,
		        .info    = std::format("channel count N={}", channels),
		    }
		);
	}

	if (channels != STBI_rgb_alpha)
	{
		switch (channels)
		{
			case STBI_rgb:
				PRINT_LOG("Image only has RGB channels. Alpha forced.");
				break;
			case STBI_grey_alpha:
				PRINT_LOG("Image only has grey + alpha channels. Forcing the rest.");
				break;
			case STBI_grey:
				PRINT_LOG("Image only has grey channel. Forcing the rest.");
				break;
			default:
				CHECK_UNREACHABLE();
		}
	}

	const ImageDimensions dims = {
	    .width    = static_cast<uint32_t>(x),
	    .height   = static_cast<uint32_t>(y),
	    .channels = static_cast<uint32_t>(STBI_rgb_alpha),
	};

	size_t bytesPerChannel = 0u;
	switch (imageComponentType)
	{
		case ComponentType::F32:
			bytesPerChannel = sizeof(float);
			break;
		case ComponentType::U16:
			bytesPerChannel = sizeof(uint16_t);
			break;
		case ComponentType::U8:
			bytesPerChannel = sizeof(uint8_t);
			break;
		default:
			CHECK_UNREACHABLE();
	}

	const size_t imageSizeInBytes = bytesPerChannel * dims.channels * dims.height * dims.width;

	ImageDescSTBI imageDescSTBI = {};

	// Copy image data.
	imageDescSTBI.data.resize(imageSizeInBytes);
	memcpy(imageDescSTBI.data.data(), decodedImagePtr, imageSizeInBytes);

	// Copy over dims and component type.
	imageDescSTBI.dims          = dims;
	imageDescSTBI.componentType = imageComponentType;

	stbi_image_free(decodedImagePtr);

	return imageDescSTBI;
}

std::optional<TextureArtifact> ImportTextureRaw(
    const std::span<const std::byte> rawBytes,
    ColorSpace requestedColorSpace,
    uint32_t requestedMipCount /* = cMaxMipCount */
)
{
	auto decodeResult = DecodeRawImageBytesWithSTBI(rawBytes);

	if (!decodeResult)
	{
		switch (decodeResult.error().errCode)
		{
			case DecodeError::FailedLoad:
				PRINT_ERROR("[STBI] Load failed: {}", decodeResult.error().info);
				break;
			case DecodeError::InvalidChannelCount:
				PRINT_ERROR("[STBI] Channel contract violation: {}", decodeResult.error().info);
				break;
		}

		return std::nullopt;
	}

	ImageDescSTBI& imageDescSTBI = decodeResult.value();
	TextureArtifact outArtifact  = {};

	// Copy/move data from STBI information to struct that is to be exported.
	outArtifact.data          = std::move(imageDescSTBI.data);
	outArtifact.componentType = imageDescSTBI.componentType;
	outArtifact.dims          = imageDescSTBI.dims;

	// Fill the remaining data.
	outArtifact.mipCount   = requestedMipCount;
	outArtifact.colorSpace = requestedColorSpace;

	return outArtifact;
}

std::optional<TextureArtifact> ImportTexture(
    const std::filesystem::path& imagePath, ColorSpace requestedColorSpace, uint32_t requestedMipCount
)
{
	const std::string imagePathStr = imagePath.string();

	PRINT_DEBUG("Importing texture from path '{}'", imagePathStr);

	auto result = Utils::ReadFileBytes(imagePath);

	if (!result)
	{
		switch (result.error())
		{
			case Utils::FileReadCode::FailedOpen:
				PRINT_ERROR("Texture file '{}' could not be opened.", imagePathStr);
				break;
			case Utils::FileReadCode::FailedRead:
				PRINT_ERROR("Texture file '{}' could not be read from.", imagePathStr);
				break;
		}

		return std::nullopt;
	}

	return ImportTextureRaw(*result, requestedColorSpace, requestedMipCount);
}
