#include "ModelImporter.h"

#include "AssetManager.h"
#include "GraphicsUtils.h"
#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "LustraLib/Utils.h"
#include "Resource.h"
#include "Sampler.h"
#include "TextureImporter.h"
#include "glm/gtc/type_ptr.hpp"
#include "tinygltf/tiny_gltf_v3.h"

#include <numbers>

using Resource::Model;

constexpr std::string_view kNotYetSupportedHeader = "[LUSTRA NOT YET SUPPORTED]";

struct AttributeIndices
{
	int32_t pos;
	int32_t uv;
	int32_t normal;
	int32_t tangent;
	int32_t color;
	int32_t index;

	// C++20 feature lets compiler figure out to compare by field (not a memcmp).
	bool operator==(const AttributeIndices& other) const = default;
};

// Create a unique cache from accessor index positions of a primitive.
struct GeomKeyHash
{
	std::size_t operator()(const AttributeIndices& atrIndices) const
	{
		size_t hash = 0;

		Utils::HashCombine(hash, static_cast<size_t>(atrIndices.pos));
		Utils::HashCombine(hash, static_cast<size_t>(atrIndices.uv));
		Utils::HashCombine(hash, static_cast<size_t>(atrIndices.normal));
		Utils::HashCombine(hash, static_cast<size_t>(atrIndices.tangent));
		Utils::HashCombine(hash, static_cast<size_t>(atrIndices.color));
		Utils::HashCombine(hash, static_cast<size_t>(atrIndices.index));

		return hash;
	}
};

// Stores the primitive range inside some source geometry given its unique set of attribute indices.
// Accessor indices maps to unique vertex sets of a primitive, ignoring its material.
using AccessorCache = std::unordered_map<AttributeIndices, Resource::PrimitiveRange, GeomKeyHash>;

// Stores a handle for each material index of a model.
using MaterialCache = std::unordered_map<int32_t, Handle<Resource::Material>>;

// Stores a handle for each texture index of a model.
// Uses uint64_t to allow unique combination of occlusion and metal+rough texture indices as an ORM texture index.
using TextureCache = std::unordered_map<uint64_t, Handle<Resource::Texture2D>>;

struct ProcessModelStatics
{
	const std::filesystem::path& baseFilePath;
	const std::string& name;
	const tg3_model& tg3Model;
	std::vector<Resource::Vertex>& vertices;
	std::vector<uint32_t>& indices;
	std::vector<Resource::MeshInstance>& instances;
	AccessorCache& accessorCache;
	MaterialCache& matCache;
	TextureCache& texCache;
};

namespace
{
	// tg3 strings are not null terminated so make sure it is seen as a view.
	// Useful when using for non-UB console output.
	std::string_view tg3StrToStrview(const tg3_str& tg3Str)
	{
		return std::string_view(tg3Str.data, tg3Str.len);
	}

	inline glm::mat4 tg3MatToGLMMAt(const double matrix[16])
	{
		// Explicit converting from a double mat4 to float mat4.
		return glm::mat4(glm::make_mat4(matrix));
	}

	Resource::SamplerDesc2D tg3SamplerToSamplerDesc(const tg3_sampler& tg3Sampler)
	{
		auto tg3WrapToVulkanAddressMode = [&](int32_t wrapAddressMode) -> vk::SamplerAddressMode
		{
			switch (wrapAddressMode)
			{
				case TG3_TEXTURE_WRAP_REPEAT:
					return vk::SamplerAddressMode::eRepeat;
					break;
				case TG3_TEXTURE_WRAP_CLAMP_TO_EDGE:
					return vk::SamplerAddressMode::eClampToEdge;
					break;
				case TG3_TEXTURE_WRAP_MIRRORED_REPEAT:
					return vk::SamplerAddressMode::eMirroredRepeat;
					break;
				default:
					return vk::SamplerAddressMode::eRepeat;
			};
		};

		// Min filter also holds the mipmap mode.
		auto tg3FilterToVulkanMinFilter = [&](int32_t minFilter) -> std::pair<vk::Filter, vk::SamplerMipmapMode>
		{
			switch (minFilter)
			{
				case TG3_TEXTURE_FILTER_NEAREST:
					return {vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest};
				case TG3_TEXTURE_FILTER_LINEAR:
					return {vk::Filter::eLinear, vk::SamplerMipmapMode::eNearest};
				case TG3_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
					return {vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest};
				case TG3_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
					return {vk::Filter::eLinear, vk::SamplerMipmapMode::eNearest};
				case TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
					return {vk::Filter::eNearest, vk::SamplerMipmapMode::eLinear};
				case TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
				default:
					return {vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};
			}
		};

		auto tg3FilterToVulkanMagFilter = [](int32_t magFilter) -> vk::Filter
		{
			switch (magFilter)
			{
				case TG3_TEXTURE_FILTER_NEAREST:
					return vk::Filter::eNearest;
				case TG3_TEXTURE_FILTER_LINEAR:
				default:
					return vk::Filter::eLinear;
			}
		};

		auto [minFilter, mipmapMode] = tg3FilterToVulkanMinFilter(tg3Sampler.min_filter);
		bool usesMipmapMode =
		    tg3Sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST && tg3Sampler.min_filter != TG3_TEXTURE_FILTER_LINEAR;

		if (mipmapMode == vk::SamplerMipmapMode::eNearest)
		{
			PRINT_WARNING(
			    "Sampler was requested to use MIPMAP nearest but will be ignored as Lustra decides to use trilinear "
			    "filtering unless disabled through global settings."
			);
		}

		Resource::SamplerDesc2D samplerDesc = {
		    .magFilter      = tg3FilterToVulkanMagFilter(tg3Sampler.mag_filter),
		    .minFilter      = minFilter,
		    .addressModeU   = tg3WrapToVulkanAddressMode(tg3Sampler.wrap_s),
		    .addressModeV   = tg3WrapToVulkanAddressMode(tg3Sampler.wrap_t),
		    .usesMipmapMode = usesMipmapMode,
		};

		return samplerDesc;
	} // namespace

	[[nodiscard]] Handle<Resource::Texture2D> GetDefaultTexture(Resource::Material::MapType mapType)
	{
		static Handle<Resource::Texture2D> defaultAlbedo   = nullhandle;
		static Handle<Resource::Texture2D> defaultNormal   = nullhandle;
		static Handle<Resource::Texture2D> defaultEmissive = nullhandle;
		static Handle<Resource::Texture2D> defaultORM      = nullhandle;

		const uint32_t defaultTexSize   = 256u;
		const size_t textureSizeInBytes = 4ull * defaultTexSize * defaultTexSize;

		Resource::TextureDesc2D texDesc = {
		    .width     = defaultTexSize,
		    .height    = defaultTexSize,
		    .format    = vk::Format::eUndefined, // Has to be defined per map type.
		    .usage     = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		    .mipLevels = cMaxMipCount
		};

		Handle<Resource::Texture2D> returnHandle = nullhandle;

		switch (mapType)
		{
			case Resource::Material::MapType::Albedo:
				if (defaultAlbedo == nullhandle)
				{
					// TODO: Implement a way to use the checker texture for albedo only when texture is unresolvable vs
					// simply not slotted through an index of -1

					// Full white default albedo.
					std::vector<std::byte> albedoColors(textureSizeInBytes, std::byte(255u));

					defaultAlbedo = Resource::AllocateNonOwning<Resource::Texture2D>();

					texDesc.format = vk::Format::eR8G8B8A8Srgb;
					Resource::CreateReadOnlyTexture2D("Default Albedo", defaultAlbedo, texDesc, albedoColors);
				}
				returnHandle = defaultAlbedo;
				break;

			case Resource::Material::MapType::Normal:
				if (defaultNormal == nullhandle)
				{
					std::vector<std::byte> normalColors(textureSizeInBytes);

					// Writes the standard blue where tangent vectors point straight out (0, 0, 1).
					for (uint32_t i = 0; i < normalColors.size(); i += 4)
					{
						normalColors[i + 0] = std::byte(128u);
						normalColors[i + 1] = std::byte(128u);
						normalColors[i + 2] = std::byte(255u);
						normalColors[i + 3] = std::byte(255u);
					}

					defaultNormal = Resource::AllocateNonOwning<Resource::Texture2D>();

					texDesc.format = vk::Format::eR8G8B8A8Unorm;
					Resource::CreateReadOnlyTexture2D("Default Normal", defaultNormal, texDesc, normalColors);
				}
				returnHandle = defaultNormal;
				break;

			case Resource::Material::MapType::Emissive:
				if (defaultEmissive == nullhandle)
				{
					// Black by default: no light emission.
					std::vector<std::byte> emissiveColor(textureSizeInBytes, std::byte(0u));

					// Write alpha so texture is visible.
					for (uint32_t i = 0; i < emissiveColor.size(); i += 4)
					{
						emissiveColor[i + 3] = std::byte(255u);
					}

					defaultEmissive = Resource::AllocateNonOwning<Resource::Texture2D>();

					texDesc.format = vk::Format::eR8G8B8A8Srgb;
					Resource::CreateReadOnlyTexture2D("Default Emissive", defaultEmissive, texDesc, emissiveColor);
				}
				returnHandle = defaultEmissive;
				break;

			case Resource::Material::MapType::ORM:
				if (defaultORM == nullhandle)
				{
					// Everything at 1. No occlusion and full alpha.
					// Let default material properties for roghness and metallic scale their values.
					std::vector<std::byte> ormColors(textureSizeInBytes, std::byte(255u));

					defaultORM = Resource::AllocateNonOwning<Resource::Texture2D>();

					texDesc.format = vk::Format::eR8G8B8A8Unorm;
					Resource::CreateReadOnlyTexture2D("Default ORM", defaultORM, texDesc, ormColors);
				}
				returnHandle = defaultORM;
				break;
		}

		return returnHandle;
	};

	[[nodiscard]] Handle<Resource::Material> GetDefaultMaterial()
	{
		static Handle<Resource::Material> defaultMaterial = nullhandle;

		if (defaultMaterial == nullhandle)
		{
			const Resource::MaterialProperties props = {}; // Default
			const Resource::Material::Maps maps      = {
			    .albedo   = Resource::AddRef(GetDefaultTexture(Resource::Material::MapType::Albedo)),
			    .normal   = Resource::AddRef(GetDefaultTexture(Resource::Material::MapType::Normal)),
			    .emissive = Resource::AddRef(GetDefaultTexture(Resource::Material::MapType::Emissive)),
			    .orm      = Resource::AddRef(GetDefaultTexture(Resource::Material::MapType::ORM)),
			};

			const Handle<Resource::Material> matHandle = Resource::AllocateNonOwning<Resource::Material>();

			Resource::CreateMaterial("Default Material", matHandle, props, maps);

			defaultMaterial = matHandle;
		}

		return defaultMaterial;
	}

	[[nodiscard]] Handle<Model> GetFallbackModel()
	{
		static Handle<Model> fallbackModel = nullhandle;

		if (fallbackModel == nullhandle)
		{
			const float n           = std::numbers::inv_sqrt3_v<float>; // 1/sqrt(3)
			const glm::vec4 magenta = {1.0f, 0.0f, 1.0f, 1.0f};

			// NOLINTBEGIN(modernize-use-designated-initializers)
			std::vector<Resource::Vertex> vertices = {
			    {{-0.5f, -0.5f, 0.5f}, {}, {-n, -n, n}, {}, magenta},   // 0
			    {{0.5f, -0.5f, 0.5f}, {}, {n, -n, n}, {}, magenta},     // 1
			    {{-0.5f, 0.5f, 0.5f}, {}, {-n, n, n}, {}, magenta},     // 2
			    {{0.5f, 0.5f, 0.5f}, {}, {n, n, n}, {}, magenta},       // 3
			    {{-0.5f, -0.5f, -0.5f}, {}, {-n, -n, -n}, {}, magenta}, // 4
			    {{0.5f, -0.5f, -0.5f}, {}, {n, -n, -n}, {}, magenta},   // 5
			    {{-0.5f, 0.5f, -0.5f}, {}, {-n, n, -n}, {}, magenta},   // 6
			    {{0.5f, 0.5f, -0.5f}, {}, {n, n, -n}, {}, magenta},     // 7
			};
			// NOLINTEND(modernize-use-designated-initializers)

			std::vector<uint32_t> indices = {
			    0, 1, 3, 0, 3, 2, // front  (+z)
			    5, 4, 6, 5, 6, 7, // back   (-z)
			    1, 5, 7, 1, 7, 3, // right  (+x)
			    4, 0, 2, 4, 2, 6, // left   (-x)
			    2, 3, 7, 2, 7, 6, // top    (+y)
			    4, 5, 1, 4, 1, 0, // bottom (-y)
			};

			std::vector<Resource::MeshInstance> instances = {Resource::MeshInstance{
			    .subMeshes = {Resource::SubMesh{
			        .range =
			            {
			                .startIndex   = 0,
			                .indexCount   = static_cast<uint32_t>(indices.size()),
			                .vertexOffset = 0,
			            },
			        .mat = Resource::AddRef(GetDefaultMaterial()),
			    }},
			    .transform = glm::mat4(1.0f),
			}};

			fallbackModel = Resource::AllocateNonOwning<Model>();
			Resource::CreateModel(
			    "Fallback Model", fallbackModel, std::move(vertices), std::move(indices), std::move(instances)
			);

			PRINT_DEBUG("Created fallback model.");
		}

		return fallbackModel;
	}

	int32_t GetAccessorIndex(const tg3_primitive& prim, std::string_view attributeName)
	{
		for (uint32_t i = 0; i < prim.attributes_count; i++)
		{
			if (tg3StrToStrview(prim.attributes[i].key) == attributeName)
			{
				return prim.attributes[i].value;
			}
		}

		// Could not find the accessor index from primitive.
		return -1;
	}

	AttributeIndices GetAttributeIndices(const tg3_primitive& primitive)
	{
		AttributeIndices atrIndices = {};
		atrIndices.pos              = GetAccessorIndex(primitive, "POSITION");
		atrIndices.uv               = GetAccessorIndex(primitive, "TEXCOORD_0");
		atrIndices.normal           = GetAccessorIndex(primitive, "NORMAL");
		atrIndices.tangent          = GetAccessorIndex(primitive, "TANGENT");
		atrIndices.color            = GetAccessorIndex(primitive, "COLOR_0");
		atrIndices.index            = primitive.indices;

		return atrIndices;
	}

	// Returns data pointer and stride or nullptr and -1 if a fault occurred.
	std::pair<const uint8_t*, uint32_t> GetAttributeDataPtr(const tg3_model& model, int32_t accessorIndex)
	{
		if (accessorIndex == -1)
		{
			return {nullptr, -1};
		}

		const tg3_accessor accessor = model.accessors[accessorIndex];

		const int32_t buffViewIndex = accessor.buffer_view;
		if (buffViewIndex < 0)
		{
			return {nullptr, -1};
		}

		const tg3_buffer_view& buffView = model.buffer_views[buffViewIndex];
		const tg3_buffer& buff          = model.buffers[buffView.buffer];

		// Handles tightly packed data (stride = 0)
		const int32_t stride = tg3_accessor_byte_stride(&accessor, &buffView);
		ENSURE(stride != -1);

		return {buff.data.data + buffView.byte_offset + accessor.byte_offset, stride};
	}

	// Will check for the source image given a texture index.
	// NOTE: Do NOT supply any other index than a texture index, as that will be UB.
	std::string GetImageName(int32_t textureIndex, const tg3_model& model)
	{
		if (textureIndex == -1)
		{
			return "Null Image";
		}

		const tg3_texture& tex = model.textures[textureIndex];

		if (tex.source == -1 || model.images[tex.source].name.len == 0)
		{
			return std::format("Tex{}", textureIndex);
		}

		return std::string(tg3StrToStrview(model.images[tex.source].name));
	}

	std::string GetMaterialName(int32_t materialIndex, const tg3_model& model)
	{
		if (materialIndex == -1)
		{
			return "Null Material";
		}

		const tg3_material& mat = model.materials[materialIndex];

		if (mat.name.len == 0)
		{
			return std::format("Mat{}", materialIndex);
		}

		return std::string(tg3StrToStrview(mat.name));
	}
} // namespace

Resource::PrimitiveRange GetPrimitiveRange(AttributeIndices atrIndicies, ProcessModelStatics& statics)
{
	// If this primtives points to a unique set of geometry, process and add the vertices to the source geometry
	// vertex/index buffers.
	if (!statics.accessorCache.contains(atrIndicies))
	{
		const tg3_accessor pos = statics.tg3Model.accessors[atrIndicies.pos];

		// The spec should ensure this.
		ENSURE(pos.type == TG3_TYPE_VEC3 || pos.type == TG3_COMPONENT_TYPE_FLOAT);

		if (pos.type == TG3_COMPONENT_TYPE_FLOAT)
		{
			PRINT_ERROR("{}: single float value positions. Only 3D points supported.", kNotYetSupportedHeader);
			ENSURE(false); // TODO: Handle actual error return values.
		}

		auto [posData, posStride]         = GetAttributeDataPtr(statics.tg3Model, atrIndicies.pos);
		auto [uvData, uvStride]           = GetAttributeDataPtr(statics.tg3Model, atrIndicies.uv);
		auto [normalData, normalStride]   = GetAttributeDataPtr(statics.tg3Model, atrIndicies.normal);
		auto [tangentData, tangentStride] = GetAttributeDataPtr(statics.tg3Model, atrIndicies.tangent);
		auto [colorData, colorStride]     = GetAttributeDataPtr(statics.tg3Model, atrIndicies.color);

		// The counts for all other attributes are guaranteed to be the same.
		const uint64_t vertexCount = pos.count;
		statics.vertices.reserve(statics.vertices.size() + vertexCount);

		Resource::PrimitiveRange primRange = {};
		// Vertices of this primitive starts where the last one was added.
		primRange.vertexOffset = static_cast<int32_t>(statics.vertices.size());

		// Add vertices to vertex buffer.
		for (uint64_t i = 0; i < vertexCount; i++)
		{
			Resource::Vertex v = {};

			memcpy(&v.pos, posData + (i * posStride), 12);

			if (uvData != nullptr)
			{
				memcpy(&v.uv, uvData + (i * uvStride), 8);
			}

			if (normalData != nullptr)
			{
				memcpy(&v.normal, normalData + (i * normalStride), 12);
			}

			if (tangentData != nullptr)
			{
				memcpy(&v.tangent, tangentData + (i * tangentStride), 12);
			}

			if (colorData != nullptr)
			{
				memcpy(&v.col, colorData + (i * colorStride), 16);
			}

			// Vertex is a trivial POD, no need to move.
			statics.vertices.emplace_back(v);
		}

		// NOTE: The spec guarantees that the index accessor is relative to the vertex attribute accessors.
		// This means that the index buffer indices start at 0, so no remapping is required between our
		// vertex/index buffer and the glTF model ones.
		const tg3_accessor& indexAccessor = statics.tg3Model.accessors[atrIndicies.index];
		auto [indexData, indexStride]     = GetAttributeDataPtr(statics.tg3Model, atrIndicies.index);

		const uint64_t indexCount = indexAccessor.count;
		statics.indices.reserve(statics.indices.size() + indexCount);

		// Indicies of this primitive starts where the last one was added.
		primRange.startIndex = static_cast<uint32_t>(statics.indices.size());
		primRange.indexCount = static_cast<uint32_t>(indexCount);

		// Add indices to index buffer.
		switch (indexAccessor.component_type)
		{
			case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
				for (size_t i = 0; i < indexCount; i++)
				{
					statics.indices.push_back(indexData[i]);
				}
				break;
			case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
				for (size_t i = 0; i < indexCount; i++)
				{
					statics.indices.push_back(reinterpret_cast<const uint16_t*>(indexData)[i]);
				}
				break;
			case TG3_COMPONENT_TYPE_UNSIGNED_INT:
				for (size_t i = 0; i < indexCount; i++)
				{
					statics.indices.push_back(reinterpret_cast<const uint32_t*>(indexData)[i]);
				}
				break;
			default:
				CHECK_UNREACHABLE();
		}

		// Add the primitive range to the accessor cache.
		statics.accessorCache.emplace(atrIndicies, primRange);
	}
	else
	{
		static uint32_t count = 0;
		PRINT_DEBUG("Accessor cache hit (N={})!", ++count);
	}

	return statics.accessorCache[atrIndicies];
}

std::optional<TextureArtifact> ResolveTextureArtifact(
    int32_t texIndex, ColorSpace colorSpace, ProcessModelStatics& statics
)
{
	ENSURE_EX(texIndex != -1, "Should never be allowed to be called with -1 as index.");

	const tg3_model& tg3Model = statics.tg3Model;
	const tg3_texture& tex    = tg3Model.textures[texIndex];

	if (tex.source == -1)
	{
		// TODO: Check extensions and see if source is there.
		return std::nullopt;
	}

	const tg3_image& image = tg3Model.images[tex.source];

	if (image.as_is == 0)
	{
		return std::nullopt;
	}

	std::optional<TextureArtifact> texArtifact = {};

	if (image.buffer_view != -1)
	{
		const tg3_buffer_view& buffView = tg3Model.buffer_views[image.buffer_view];

		const std::span<const std::byte> imageSpan(
		    reinterpret_cast<const std::byte*>(tg3Model.buffers[buffView.buffer].data.data) + buffView.byte_offset,
		    buffView.byte_length
		);

		texArtifact = ImportTextureRaw(imageSpan, colorSpace);
	}
	else
	{
		ENSURE(image.uri.data != nullptr);

		const std::string_view uri = tg3StrToStrview(image.uri);

		texArtifact = ImportTexture(statics.baseFilePath / uri, colorSpace);
	}

	return texArtifact;
}

tg3_sampler GetSampler(int32_t texIndex, const tg3_model& tg3Model)
{
	const tg3_texture& tex = tg3Model.textures[texIndex];

	tg3_sampler sampler = {};

	// Default sampler wrap must be REPEAT according to glTF spec.
	sampler.wrap_s = TG3_TEXTURE_WRAP_REPEAT;
	sampler.wrap_t = TG3_TEXTURE_WRAP_REPEAT;

	// This is engine choice.
	sampler.min_filter = -1;
	sampler.mag_filter = -1;

	if (tex.sampler != -1)
	{
		sampler = tg3Model.samplers[tex.sampler];
	}

	return sampler;
}

Handle<Resource::Texture2D> GetSimpleTextureHandle(
    const std::string& materialName, int32_t texIndex, ProcessModelStatics& statics, Resource::Material::MapType mapType
)
{
	if (texIndex == -1)
	{
		return GetDefaultTexture(mapType);
	}

	const uint64_t texKey = static_cast<uint64_t>(texIndex);

	if (!statics.texCache.contains(texKey))
	{
		ColorSpace colorSpace = ColorSpace::Unknown;
		std::string mapName;
		switch (mapType)
		{
			case Resource::Material::MapType::Emissive:
				mapName    = "Emissive";
				colorSpace = ColorSpace::sRGB;
				break;

			case Resource::Material::MapType::Albedo:
				mapName    = "Albedo";
				colorSpace = ColorSpace::sRGB;
				break;

			case Resource::Material::MapType::Normal:
				mapName    = "Normal";
				colorSpace = ColorSpace::Linear;
				break;

			case Resource::Material::MapType::ORM:
				// TODO: Handle this better as ORM should not be allowed to be sent into this function.
				CHECK_UNREACHABLE();
				break;
		}

		std::optional<TextureArtifact> result = ResolveTextureArtifact(texIndex, colorSpace, statics);

		Handle<Resource::Texture2D> texHandle = {};
		if (!result)
		{
			PRINT_DEBUG("Could not resolve map for map type '{}'. Using missing texture.", (uint8_t)mapType);
			texHandle = Resource::GetMissingTexture();
		}
		else
		{
			const TextureArtifact& texArtifact = result.value();

			vk::Format format = vk::Format::eUndefined;
			switch (texArtifact.componentType)
			{
				case ComponentType::U8:
					format = colorSpace == ColorSpace::sRGB ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
					break;
				case ComponentType::U16:
					format = vk::Format::eR16G16B16A16Unorm;
					break;
				case ComponentType::F32:
					format = vk::Format::eR32G32B32A32Sfloat;
					break;
				case ComponentType::Unknown:
					CHECK_UNREACHABLE();
			}

			const tg3_sampler sampler = GetSampler(texIndex, statics.tg3Model);
			auto samplerDesc          = tg3SamplerToSamplerDesc(sampler);
			UNUSED_VAR(samplerDesc); // TODO: Put sampler instantiation in the correct place of model importing.

			Resource::TextureDesc2D texDesc = {
			    .width     = texArtifact.dims.width,
			    .height    = texArtifact.dims.height,
			    .format    = format,
			    .usage     = vk::ImageUsageFlagBits::eSampled,
			    .mipLevels = texArtifact.mipCount,
			};

			texHandle = Resource::AllocateNonOwning<Resource::Texture2D>();
			Resource::CreateReadOnlyTexture2D(
			    std::format(
			        "{}/{}/{}/{}", statics.name, materialName, mapName, GetImageName(texIndex, statics.tg3Model)
			    ),
			    texHandle,
			    texDesc,
			    texArtifact.data
			);
		}

		statics.texCache.emplace(texKey, texHandle);
	}

	else
	{
		static uint32_t count = 0;
		PRINT_DEBUG("Texture cache hit (N={})!", ++count);
	}

	return statics.texCache[texKey];
}

Handle<Resource::Texture2D> GetORMTextureHandle(
    const std::string& matName, int32_t occlusionTexIndex, int32_t metalRoughTexIndex, ProcessModelStatics& statics
)
{
	if (occlusionTexIndex == -1 && metalRoughTexIndex == -1)
	{
		return GetDefaultTexture(Resource::Material::MapType::ORM);
	}

	// Takes two int32s and puts them in the two 32 bit parts of a uint64. Initial uint32_t cast to allow index of
	// -1 to be a valid key. Endianness decides if numbers are put in lower or higher end of those 64 bits, but the
	// unique number produced is what is essential.
	const uint64_t ormTexKey = (static_cast<uint64_t>(static_cast<uint32_t>(occlusionTexIndex)) << 32) |
	                           (static_cast<uint64_t>(static_cast<uint32_t>(metalRoughTexIndex)));

	if (!statics.texCache.contains(ormTexKey))
	{
		const ColorSpace colorSpace = ColorSpace::Linear;

		// Will  with the artifact that will contain the ORM information.
		TextureArtifact finalTexArtifact = {};

		if (occlusionTexIndex == -1)
		{
			std::optional<TextureArtifact> metalRoughResult =
			    ResolveTextureArtifact(metalRoughTexIndex, colorSpace, statics);

			if (metalRoughResult)
			{
				TextureArtifact& metalRoughTexArtifact = metalRoughResult.value();

				// Write default data for occlusion channel.
				for (size_t i = 0; i < metalRoughTexArtifact.data.size(); i += 4)
				{
					// R = Occlusion
					metalRoughTexArtifact.data[i] = std::byte(255u); // Equates to no ambient occlusion
				}

				finalTexArtifact = std::move(metalRoughTexArtifact);
			}
		}
		else if (metalRoughTexIndex == -1)
		{
			std::optional<TextureArtifact> occlusionResult =
			    ResolveTextureArtifact(occlusionTexIndex, colorSpace, statics);

			if (occlusionResult)
			{
				TextureArtifact& occlusionTexArtifact = occlusionResult.value();

				// Write default data for roughness + metallic channel.
				for (size_t i = 0; i < occlusionTexArtifact.data.size(); i += 4)
				{
					// G = Roughness
					occlusionTexArtifact.data[i + 1] = std::byte(255u); // Full roughness

					// B = Metallic
					occlusionTexArtifact.data[i + 2] = std::byte(0u); // No metallic properties
				}

				finalTexArtifact = std::move(occlusionTexArtifact);
			}
		}
		else // Both texture indices exist.
		{
			std::optional<TextureArtifact> metalRoughResult =
			    ResolveTextureArtifact(metalRoughTexIndex, colorSpace, statics);

			// If they point to the same texture, ORM is already encoded in the texture that both indices point at.
			if (metalRoughResult && metalRoughTexIndex == occlusionTexIndex)
			{
				PRINT_DEBUG("AO and MR index are the same. ORM texture passed along directly.");

				finalTexArtifact = std::move(metalRoughResult.value());
			}
			else
			{
				std::optional<TextureArtifact> occlusionResult =
				    ResolveTextureArtifact(occlusionTexIndex, colorSpace, statics);

				TextureArtifact& metalRoughTexArtifact = metalRoughResult.value();
				TextureArtifact& occlusionTexArtifact  = occlusionResult.value();

				ENSURE(metalRoughTexArtifact.channels == 4 && occlusionTexArtifact.channels == 4);

				// Guaranteed by the spec (unless extensions are used).
				ENSURE(
				    metalRoughTexArtifact.componentType == ComponentType::U8 &&
				    occlusionTexArtifact.componentType == ComponentType::U8
				);

				const bool sameDims = (metalRoughTexArtifact.dims == occlusionTexArtifact.dims);

				if (!sameDims)
				{
					PRINT_DEBUG(
					    "ORM texture dimensions does not match with each other. Upscaling smaller tex to "
					    "fit in the larger one."
					);

					// For now only supports aspect ratios of 1:1 (square).
					ENSURE(metalRoughTexArtifact.dims.width == metalRoughTexArtifact.dims.height);
					ENSURE(occlusionTexArtifact.dims.width == occlusionTexArtifact.dims.height);

					const uint32_t largestDim =
					    std::max(metalRoughTexArtifact.dims.width, occlusionTexArtifact.dims.width);

					const bool occlusionSmaller = occlusionTexArtifact.dims.width < metalRoughTexArtifact.dims.width;
					TextureArtifact& largerTex  = occlusionSmaller ? metalRoughTexArtifact : occlusionTexArtifact;
					const TextureArtifact& smallerTex = occlusionSmaller ? occlusionTexArtifact : metalRoughTexArtifact;

					glm::u8vec4* largerTexPixels = reinterpret_cast<glm::u8vec4*>(largerTex.data.data());

					// Upsample the smaller texture to the dimensions of the larger texture to not loose any
					// information/detail.
					for (uint32_t y = 0; y < largestDim; y++)
					{
						for (uint32_t x = 0; x < largestDim; x++)
						{
							// Offset by half a pixel.
							const glm::vec2 uv = (glm::vec2(x, y) + 0.5f) / static_cast<float>(largestDim);

							const glm::u8vec4 sample = GraphicsUtils::SampleBilinearU8(
							    smallerTex.data, uv, smallerTex.dims.width, smallerTex.dims.height, smallerTex.channels
							);

							const uint32_t index1D = x + (y * largestDim);

							// Write to specific channels because spec does not guarantee that textures dont have
							// overlapping, but redundant, color values.
							if (occlusionSmaller)
							{
								largerTexPixels[index1D].r = sample.r;
							}
							else
							{
								largerTexPixels[index1D].g = sample.g;
								largerTexPixels[index1D].b = sample.b;
							}
						}
					}

					finalTexArtifact = std::move(largerTex);
				}
				else
				{
					// Write occlusion into roughness and metallic buffer.
					// Only loops data from the RED channel (assuming U8 encoding).
					for (size_t i = 0; i < metalRoughTexArtifact.data.size(); i += 4)
					{
						metalRoughTexArtifact.data[i] = occlusionTexArtifact.data[i];
					}

					finalTexArtifact = std::move(metalRoughTexArtifact);
				}
			}
		}

		Handle<Resource::Texture2D> texHandle = {};
		if (finalTexArtifact.data.empty())
		{
			PRINT_DEBUG("Could not resolve data for ORM texture. Using missing texture.");
			texHandle = Resource::GetMissingTexture();
		}
		else
		{
			Resource::TextureDesc2D texDesc = {
			    .width     = finalTexArtifact.dims.width,
			    .height    = finalTexArtifact.dims.height,
			    .format    = vk::Format::eR8G8B8A8Unorm, // Guaranteed by the spec (unless extensions are used).
			    .usage     = vk::ImageUsageFlagBits::eSampled,
			    .mipLevels = finalTexArtifact.mipCount,
			};

			texHandle = Resource::AllocateNonOwning<Resource::Texture2D>();
			Resource::CreateReadOnlyTexture2D(
			    std::format(
			        "{}/{}/ORMTex/({}, {})",
			        statics.name,
			        matName,
			        GetImageName(occlusionTexIndex, statics.tg3Model),
			        GetImageName(metalRoughTexIndex, statics.tg3Model)
			    ),
			    texHandle,
			    texDesc,
			    finalTexArtifact.data
			);
		}

		statics.texCache.emplace(ormTexKey, texHandle);
	}
	else
	{
		static uint32_t count = 0;
		PRINT_DEBUG("Texture cache hit (N={})!", ++count);
	}

	return statics.texCache.at(ormTexKey);
}

Handle<Resource::Material> GetMaterialHandle(int32_t matIndex, ProcessModelStatics& statics)
{
	if (matIndex == -1)
	{
		return GetDefaultMaterial();
	}

	else if (!statics.matCache.contains(matIndex))
	{
		const tg3_material& material = statics.tg3Model.materials[matIndex];
		// TODO: Check for texcoord for the occlusion tex.

		const int32_t normalTexIndex     = material.normal_texture.index;
		const int32_t emissiveTexIndex   = material.emissive_texture.index;
		const int32_t occlusionTexIndex  = material.occlusion_texture.index;
		const int32_t albedoTexIndex     = material.pbr_metallic_roughness.base_color_texture.index;
		const int32_t metalRoughTexIndex = material.pbr_metallic_roughness.metallic_roughness_texture.index;

		std::string matName = GetMaterialName(matIndex, statics.tg3Model);

		const Resource::Material::Maps maps = {
		    .albedo = Resource::AddRef(
		        GetSimpleTextureHandle(matName, albedoTexIndex, statics, Resource::Material::MapType::Albedo)
		    ),
		    .normal = Resource::AddRef(
		        GetSimpleTextureHandle(matName, normalTexIndex, statics, Resource::Material::MapType::Normal)
		    ),
		    .emissive = Resource::AddRef(
		        GetSimpleTextureHandle(matName, emissiveTexIndex, statics, Resource::Material::MapType::Emissive)
		    ),
		    .orm = Resource::AddRef(GetORMTextureHandle(matName, occlusionTexIndex, metalRoughTexIndex, statics)),
		};

		const Resource::MaterialProperties props = {
		    .albedoFactor      = glm::make_vec4(material.pbr_metallic_roughness.base_color_factor),
		    .emissiveFactor    = glm::make_vec3(material.emissive_factor),
		    .occlusionStrength = static_cast<float>(material.occlusion_texture.strength),
		    .metallicFactor    = static_cast<float>(material.pbr_metallic_roughness.metallic_factor),
		    .roughnessFactor   = static_cast<float>(material.pbr_metallic_roughness.roughness_factor),
		};

		const Handle<Resource::Material> matHandle = Resource::AllocateNonOwning<Resource::Material>();
		Resource::CreateMaterial(matName, matHandle, props, maps);

		statics.matCache.emplace(matIndex, matHandle);
	}
	else
	{
		static uint32_t count = 0;
		PRINT_DEBUG("Material cache hit (N={})!", ++count);
	}

	return statics.matCache.at(matIndex);
}

[[nodiscard]] bool ProcessModel(ProcessModelStatics& statics, int32_t nodeIndex, glm::mat4 parentTransform)
{
	const tg3_node& node = statics.tg3Model.nodes[nodeIndex];
	PRINT_DEBUG("Processing node '{}'", tg3StrToStrview(node.name));

	auto localTransform = glm::mat4(1.0f);
	if (node.has_matrix != 0)
	{
		localTransform = parentTransform * tg3MatToGLMMAt(node.matrix);
	}
	else
	{
		const glm::vec3 scale = glm::make_vec3(node.scale);
		const glm::quat quat  = glm::make_quat(node.rotation);
		const glm::vec3 pos   = glm::make_vec3(node.translation);

		const glm::mat4 transform =
		    glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(quat) * glm::scale(glm::mat4(1.0f), scale);

		localTransform = parentTransform * transform;
	}

	if (node.mesh != -1)
	{
		const tg3_mesh& tg3Mesh = statics.tg3Model.meshes[node.mesh];

		PRINT_DEBUG(
		    "Processing submeshes of mesh '{}' (N={})", tg3StrToStrview(tg3Mesh.name), tg3Mesh.primitives_count
		);

		Resource::MeshInstance meshInstance;
		meshInstance.transform = localTransform;

		for (uint32_t primIndex = 0; primIndex < tg3Mesh.primitives_count; primIndex++)
		{
			const tg3_primitive& primitive = tg3Mesh.primitives[primIndex];

			const AttributeIndices atrIndicies = GetAttributeIndices(primitive);

			Resource::PrimitiveRange range = GetPrimitiveRange(atrIndicies, statics);
			Handle<Resource::Material> mat = Resource::AddRef(GetMaterialHandle(primitive.material, statics));

			meshInstance.subMeshes.emplace_back(
			    Resource::SubMesh{
			        .range = range,
			        .mat   = mat,
			    }
			);
		}

		statics.instances.push_back(std::move(meshInstance));
	}

	// Recursively go through next set of nodes.
	if (node.children_count != 0)
	{
		for (uint32_t i = 0; i < node.children_count; i++)
		{
			if (!ProcessModel(statics, node.children[i], localTransform))
			{
				return false;
			}
		}
	}

	return true;
}

// Returns true if model could be fully resolved.
// Input model is assumed to have at least one scene containing a scene node.
[[nodiscard]] bool ResolveModel(
    const std::string& fileName,
    const std::filesystem::path& baseFilePath,
    const tg3_model& tg3Model,
    std::vector<Resource::Vertex>& vertices,
    std::vector<uint32_t>& indices,
    std::vector<Resource::MeshInstance>& instances
)
{
	// Validate if glTF model is fit for parsing.
	{
		if (tg3Model.scenes_count > 1)
		{
			PRINT_ERROR("{}: parsing of more than one scene per file.", kNotYetSupportedHeader);
			return false;
		}

		for (uint32_t i = 0; i < tg3Model.accessors_count; i++)
		{
			if (tg3Model.accessors[i].sparse.is_sparse == 1)
			{
				PRINT_ERROR("{}: sparse vertex attribute accessors.", kNotYetSupportedHeader);
				return false;
			}
		}

		// Go through all meshes and see that they have an associated index buffer.
		for (uint32_t meshIndex = 0; meshIndex < tg3Model.meshes_count; meshIndex++)
		{
			const tg3_mesh& mesh = tg3Model.meshes[meshIndex];

			for (uint32_t primIndex = 0; primIndex < mesh.primitives_count; primIndex++)
			{
				if (mesh.primitives[primIndex].indices == -1)
				{
					PRINT_ERROR("{}: non index buffered meshes.", kNotYetSupportedHeader);
					return false;
				}
			}
		}

		if (tg3Model.animations_count != 0)
		{
			PRINT_WARNING(
			    "{}: model animations. Parsing will continue without processing animations.", kNotYetSupportedHeader
			);
		}

		// TODO: Check for extensions that are not supported.
	}

	AccessorCache accessorCache = {};
	MaterialCache matCache      = {};
	TextureCache texCache       = {};

	ProcessModelStatics statics = {
	    .baseFilePath  = baseFilePath,
	    .name          = fileName,
	    .tg3Model      = tg3Model,
	    .vertices      = vertices,
	    .indices       = indices,
	    .instances     = instances,
	    .accessorCache = accessorCache,
	    .matCache      = matCache,
	    .texCache      = texCache,
	};

	auto parentMatrix = glm::mat4(1.0f);
	return ProcessModel(statics, 0, parentMatrix);
}

Handle<Model> AssetImporter<Model>::Import(const AssetEntry& modelEntry)
{
	const auto& modelMetadata = AssetManager::GetMetadata<Metadata::Model>(modelEntry);
	UNUSED_VAR(modelMetadata);

	tg3_parse_options opts;
	tg3_error_stack errors;
	tg3_model gltfModel;

	tg3_parse_options_init(&opts);
	tg3_error_stack_init(&errors);

	// Requires the gltf version to be 2.0.
	// Requires the file to have a scene and node structure.
	opts.required_sections = TG3_REQUIRE_VERSION | TG3_REQUIRE_SCENES | TG3_REQUIRE_NODES;

	// JSON parses numbers as 32 bit floats for performance.
	// Very rarely does a gltf file include anything else than single precision formats for floats.
	opts.parse_float32 = 1;

	// This does not really do anything for tg3, it simply forces the as_is value for images to be 1.
	// To actually fetch data, check the buffer view for an image or the URI if that doesnt exist.
	opts.images_as_is = 1;

	std::string assetPath = modelEntry.assetPath.string();
	const tg3_error_code err =
	    tg3_parse_file(&gltfModel, &errors, assetPath.data(), static_cast<uint32_t>(assetPath.length()), &opts);

	// Used to ductape fix a bug where tg3 errors on an import but seemingly still allocates an arena that throws a
	// segfault when attempted to be freed.
	bool fileNotFound = false;

	// Unallocated model handle.
	Handle<Model> outModelHandle = {};
	if (err != TG3_OK)
	{
		PRINT_ERROR("--- Error importing gltf file '{}' ---", assetPath);
		for (uint32_t i = 0; i < errors.count; i++)
		{
			const tg3_error_entry* errEntry = tg3_errors_get(&errors, i);

			LustraLib::OutputLevel outputLevel;
			switch (errEntry->severity)
			{
				case TG3_SEVERITY_ERROR:
					outputLevel = LustraLib::OutputLevelError;
					break;
				case TG3_SEVERITY_WARNING:
					outputLevel = LustraLib::OutputLevelWarning;
					break;
				case TG3_SEVERITY_INFO:
					outputLevel = LustraLib::OutputLevelLog;
					break;
				default:
					CHECK_UNREACHABLE();
			}

			PRINT_CUSTOM(
			    outputLevel,
			    "[{}] {} at {}",
			    static_cast<int>(errEntry->code),
			    errEntry->message ? errEntry->message : "(no message)",
			    errEntry->json_path ? errEntry->json_path : "(unknown path)"
			);

			if (errEntry->code == TG3_ERR_FILE_NOT_FOUND)
			{
				fileNotFound = true;
			}
		}

		PRINT_ERROR("Failed parsing model from '{}' through TG3. Using fallback model.", assetPath);
		outModelHandle = GetFallbackModel();
	}
	else
	{
		std::vector<Resource::Vertex> vertices            = {};
		std::vector<uint32_t> indices                     = {};
		std::vector<Resource::MeshInstance> meshInstances = {};

		const std::filesystem::path baseFilePath = modelEntry.assetPath.parent_path();
		const std::string fileName               = modelEntry.assetPath.stem().string(); // Filename sans extension.

		if (!ResolveModel(fileName, baseFilePath, gltfModel, vertices, indices, meshInstances))
		{
			PRINT_ERROR("Failed resolving model from '{}'. Using fallback model.", assetPath);
			outModelHandle = GetFallbackModel();
		}
		else
		{
			outModelHandle = Resource::AllocateNonOwning<Model>();
			Resource::CreateModel(
			    fileName, outModelHandle, std::move(vertices), std::move(indices), std::move(meshInstances)
			);
		}
	}

	if (!fileNotFound)
	{
		tg3_model_free(&gltfModel);
	}

	tg3_error_stack_free(&errors);

	return Resource::AddRef(outModelHandle);
}
