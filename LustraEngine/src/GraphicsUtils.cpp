#include "GraphicsUtils.h"

#include "LustraLib/Assert.h"

#include <cstddef>
#include <span>
#include <unordered_map>

namespace GraphicsUtils
{
	constexpr const char* kVulkan10FeatureNames[] = {
	    "Robust Buffer Access",
	    "Full Draw Index UInt32",
	    "Image Cube Array",
	    "Independent Blend",
	    "Geometry Shader",
	    "Tessellation Shader",
	    "Sample Rate Shading",
	    "Dual Source Blend",
	    "Logic Op",
	    "Multi Draw Indirect",
	    "Draw Indirect First Instance",
	    "Depth Clamp",
	    "Depth Bias Clamp",
	    "Fill Mode Non-Solid",
	    "Depth Bounds",
	    "Wide Lines",
	    "Large Points",
	    "Alpha To One",
	    "Multi Viewport",
	    "Sampler Anisotropy",
	    "Texture Compression ETC2",
	    "Texture Compression ASTC LDR",
	    "Texture Compression BC",
	    "Occlusion Query Precise",
	    "Pipeline Statistics Query",
	    "Vertex Pipeline Stores And Atomics",
	    "Fragment Stores And Atomics",
	    "Shader Tessellation And Geometry Point Size",
	    "Shader Image Gather Extended",
	    "Shader Storage Image Extended Formats",
	    "Shader Storage Image Multisample",
	    "Shader Storage Image Read Without Format",
	    "Shader Storage Image Write Without Format",
	    "Shader Uniform Buffer Array Dynamic Indexing",
	    "Shader Sampled Image Array Dynamic Indexing",
	    "Shader Storage Buffer Array Dynamic Indexing",
	    "Shader Storage Image Array Dynamic Indexing",
	    "Shader Clip Distance",
	    "Shader Cull Distance",
	    "Shader Float64",
	    "Shader Int64",
	    "Shader Int16",
	    "Shader Resource Residency",
	    "Shader Resource Min LOD",
	    "Sparse Binding",
	    "Sparse Residency Buffer",
	    "Sparse Residency Image 2D",
	    "Sparse Residency Image 3D",
	    "Sparse Residency 2 Samples",
	    "Sparse Residency 4 Samples",
	    "Sparse Residence 8 Samples",
	    "Sparse Residency 16 Samples",
	    "Sparse Residency Aliased",
	    "Variable Multisample Rate",
	    "Inherited Queries",
	};

	constexpr const char* kVulkan11FeatureNames[] = {
	    "Storage Buffer 16-Bit Access",
	    "Uniform And Storage Buffer 16-Bit Access",
	    "Storage Push Constant 16-Bit",
	    "Storage Input/Output 16-Bit",
	    "Multiview",
	    "Multiview Geometry Shader",
	    "Multiview Tessellation Shader",
	    "Variable Pointers Storage Buffer",
	    "Variable Pointers",
	    "Protected Memory",
	    "Sampler YCbCr Conversion",
	    "Shader Draw Parameters",
	};

	constexpr const char* kVulkan12FeatureNames[] = {
	    "Sampler Mirror Clamp To Edge",
	    "Draw Indirect Count",
	    "Storage Buffer 8-Bit Access",
	    "Uniform And Storage Buffer 8-Bit Access",
	    "Storage Push Constant 8-Bit",
	    "Shader Buffer Int64 Atomics",
	    "Shader Shared Int64 Atomics",
	    "Shader Float16",
	    "Shader Int8",
	    "Descriptor Indexing",
	    "Shader Input Attachment Array Dynamic Indexing",
	    "Shader Uniform Texel Buffer Array Dynamic Indexing",
	    "Shader Storage Texel Buffer Array Dynamic Indexing",
	    "Shader Uniform Buffer Array Non-Uniform Indexing",
	    "Shader Sampled Image Array Non-Uniform Indexing",
	    "Shader Storage Buffer Array Non-Uniform Indexing",
	    "Shader Storage Image Array Non-Uniform Indexing",
	    "Shader Input Attachment Array Non-Uniform Indexing",
	    "Shader Uniform Texel Buffer Array Non-Uniform Indexing",
	    "Shader Storage Texel Buffer Array Non-Uniform Indexing",
	    "Descriptor Binding Uniform Buffer Update After Bind",
	    "Descriptor Binding Sampled Image Update After Bind",
	    "Descriptor Binding Storage Image Update After Bind",
	    "Descriptor Binding Storage Buffer Update After Bind",
	    "Descriptor Binding Uniform Texel Buffer Update After Bind",
	    "Descriptor Binding Storage Texel Buffer Update After Bind",
	    "Descriptor Binding Update Unused While Pending",
	    "Descriptor Binding Partially Bound",
	    "Descriptor Binding Variable Descriptor Count",
	    "Runtime Descriptor Array",
	    "Sampler Filter Minmax",
	    "Scalar Block Layout",
	    "Imageless Framebuffer",
	    "Uniform Buffer Standard Layout",
	    "Shader Subgroup Extended Types",
	    "Separate Depth Stencil Layouts",
	    "Host Query Reset",
	    "Timeline Semaphore",
	    "Buffer Device Address",
	    "Buffer Device Address Capture Replay",
	    "Buffer Device Address Multi-Device",
	    "Vulkan Memory Model",
	    "Vulkan Memory Model Device Scope",
	    "Vulkan Memory Model Availability Visibility Chains",
	    "Shader Output Viewport Index",
	    "Shader Output Layer",
	    "Subgroup Broadcast Dynamic ID",
	};

	constexpr const char* kVulkan13FeatureNames[] = {
	    "Robust Image Access",
	    "Inline Uniform Block",
	    "Descriptor Binding Inline Uniform Block Update After Bind",
	    "Pipeline Creation Cache Control",
	    "Private Data",
	    "Shader Demote To Helper Invocation",
	    "Shader Terminate Invocation",
	    "Subgroup Size Control",
	    "Compute Full Subgroups",
	    "Synchronization 2",
	    "Texture Compression ASTC HDR",
	    "Shader Zero-Initialize Workgroup Memory",
	    "Dynamic Rendering",
	    "Shader Integer Dot Product",
	    "Maintenance 4",
	};

	constexpr const char* kVulkan14FeatureNames[] = {
	    "Global Priority Query",
	    "Shader Subgroup Rotate",
	    "Shader Subgroup Rotate Clustered",
	    "Shader Float Controls 2",
	    "Shader Expect Assume",
	    "Rectangular Lines",
	    "Bresenham Lines",
	    "Smooth Lines",
	    "Stippled Rectangular Lines",
	    "Stippled Bresenham Lines",
	    "Stippled Smooth Lines",
	    "Vertex Attribute Instance Rate Divisor",
	    "Vertex Attribute Instance Rate Zero Divisor",
	    "Index Type UInt8",
	    "Dynamic Rendering Local Read",
	    "Maintenance 5",
	    "Maintenance 6",
	    "Pipeline Protected Access",
	    "Pipeline Robustness",
	    "Host Image Copy",
	    "Push Descriptor",
	};

	FeatureNamesInfo GetFeatureNames(vk::StructureType structType)
	{
		static const std::unordered_map<vk::StructureType, FeatureNamesInfo> kFeatureNameTables = {
		    {vk::StructureType::ePhysicalDeviceFeatures2,
		     {.names       = kVulkan10FeatureNames,
		      .count       = std::size(kVulkan10FeatureNames),
		      .firstOffset = offsetof(VkPhysicalDeviceFeatures, robustBufferAccess)}},

		    {vk::StructureType::ePhysicalDeviceVulkan11Features,
		     {.names       = kVulkan11FeatureNames,
		      .count       = std::size(kVulkan11FeatureNames),
		      .firstOffset = offsetof(VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess)}},

		    {vk::StructureType::ePhysicalDeviceVulkan12Features,
		     {.names       = kVulkan12FeatureNames,
		      .count       = std::size(kVulkan12FeatureNames),
		      .firstOffset = offsetof(VkPhysicalDeviceVulkan12Features, samplerMirrorClampToEdge)}},

		    {vk::StructureType::ePhysicalDeviceVulkan13Features,
		     {.names       = kVulkan13FeatureNames,
		      .count       = std::size(kVulkan13FeatureNames),
		      .firstOffset = offsetof(VkPhysicalDeviceVulkan13Features, robustImageAccess)}},

		    {vk::StructureType::ePhysicalDeviceVulkan14Features,
		     {.names       = kVulkan14FeatureNames,
		      .count       = std::size(kVulkan14FeatureNames),
		      .firstOffset = offsetof(VkPhysicalDeviceVulkan14Features, globalPriorityQuery)}},
		};

		ENSURE_EX(
		    kFeatureNameTables.contains(structType), "Invalid feature structure type: {}", static_cast<int>(structType)
		);

		return kFeatureNameTables.at(structType);
	}

	inline uint32_t Pixel2DTo1D(glm::i32vec2 pixel, uint32_t width)
	{
		glm::u32vec2 unsignedPixelPos = pixel;
		return unsignedPixelPos.x + (unsignedPixelPos.y * width);
	}

	// Clamped bilinear sampling.
	// Returns a four wide vector but only samples data matching channel count.
	// Assumes linear color space.
	glm::u8vec4 SampleBilinearU8(
	    std::span<const std::byte> imageData, glm::vec2 uv, uint32_t width, uint32_t height, uint32_t channels
	)
	{
		const uint32_t pixelSizeInBytes = channels * 1u; // Assumes U8 encoding = 1 byte per channel.
		const glm::i32vec2 maxBounds    = {width - 1, height - 1};
		const glm::i32vec2 minBounds    = {0, 0};

		const glm::vec2 pixelPos   = uv * glm::vec2({width, height});
		const glm::vec2 tlPos      = pixelPos - 0.5f;
		const glm::i32vec2 basePos = glm::floor(tlPos);
		const glm::vec2 fracs      = tlPos - glm::floor(tlPos);

		// Order: TL, TR, BL, BR
		std::array<glm::i32vec2, 4> offsets = {{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};

		// Packed weights: x=TL, y=TR, z=BL, w=BR
		glm::vec4 weights = glm::vec4(
		    (1.0f - fracs.x) * (1.0f - fracs.y), // TL
		    fracs.x * (1.0f - fracs.y),          // TR
		    (1.0f - fracs.x) * fracs.y,          // BL
		    fracs.x * fracs.y                    // BR
		);

		// Combination of values happens in floating point space.
		glm::vec4 combinedColor = {};
		for (uint32_t i = 0; i < 4; i++)
		{
			glm::i32vec2 offset = offsets[i];

			glm::i32vec2 pixel = basePos + offset;

			// Uses clamp sampling
			pixel = glm::clamp(pixel, minBounds, maxBounds);

			uint32_t pixel1D = Pixel2DTo1D(pixel, width);

			const std::byte* pixelPtr = imageData.data() + (static_cast<size_t>(pixel1D) * pixelSizeInBytes);

			// Only copies the bytes the source uses.
			glm::u8vec4 texel(0.0f);
			memcpy(&texel, pixelPtr, pixelSizeInBytes);

			combinedColor += glm::vec4(texel) * weights[static_cast<int>(i)];
		}

		// Clamp to encoding range.
		combinedColor = glm::clamp(glm::round(combinedColor), 0.0f, 255.0f);

		// Final cast to input encoding.
		return glm::u8vec4(combinedColor);
	}

} // namespace GraphicsUtils
