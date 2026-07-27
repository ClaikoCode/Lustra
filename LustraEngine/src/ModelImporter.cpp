#include "ModelImporter.h"

#include "AssetManager.h"
#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "LustraLib/Utils.h"
#include "Resource.h"
#include "glm/gtc/type_ptr.hpp"
#include "tinygltf/tiny_gltf_v3.h"

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

struct ProcessModelStatics
{
	const tg3_model& tg3Model;
	std::vector<Resource::Vertex>& vertices;
	std::vector<uint32_t>& indices;
	std::vector<Resource::MeshInstance>& instances;
	AccessorCache& accessorCache;
};

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

// Goes through all primities of a mesh and sums the POSITION attribute count of each.
uint64_t MeshVertexCount(const tg3_model& model, const tg3_mesh& mesh)
{
	uint64_t vertexCount = 0;

	for (uint32_t i = 0; i < mesh.primitives_count; i++)
	{
		const tg3_primitive& prim = mesh.primitives[i];

		int32_t posIndex = GetAccessorIndex(prim, "POSITION");
		ENSURE(posIndex != -1);

		vertexCount += model.accessors[posIndex].count;
	}

	return vertexCount;
}

// Returns data pointer and stride or nullptr and -1 if a fault occurred.
std::pair<const uint8_t*, uint32_t> GetAttributeDataPtr(const tg3_model& model, int32_t accessorIndex)
{
	if (accessorIndex == -1)
	{
		return {nullptr, -1};
	}

	tg3_accessor accessor = model.accessors[accessorIndex];

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

			// If this primtives points to a unique set of geometry, process and add the vertices to the source geometry
			// vertex/index buffers.
			if (!statics.accessorCache.contains(atrIndicies))
			{
				tg3_accessor pos = statics.tg3Model.accessors[atrIndicies.pos];

				// The spec should ensure this.
				ENSURE(pos.type == TG3_TYPE_VEC3 || pos.type == TG3_COMPONENT_TYPE_FLOAT);

				if (pos.type == TG3_COMPONENT_TYPE_FLOAT)
				{
					PRINT_ERROR("{}: single float value positions. Only 3D points supported.", kNotYetSupportedHeader);
					return false;
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

			meshInstance.subMeshes.emplace_back(
			    Resource::SubMesh{
			        .range = statics.accessorCache[atrIndicies],
			        .mat   = {}, // TODO: Resolve material handle.
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
	}

	AccessorCache accessorCache = {};
	auto parentMatrix           = glm::mat4(1.0f);

	ProcessModelStatics statics = {
	    .tg3Model      = tg3Model,
	    .vertices      = vertices,
	    .indices       = indices,
	    .instances     = instances,
	    .accessorCache = accessorCache,
	};

	return ProcessModel(statics, 0, parentMatrix);
}

Handle<Model> AssetImporter<Model>::Import(const AssetEntry& modelEntry)
{
	const auto& modelMetadata = modelEntry.GetMetadata<Metadata::Model>();
	(void)(modelMetadata);

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

	std::string_view assetPath = modelEntry.assetPath.c_str();
	tg3_error_code err =
	    tg3_parse_file(&gltfModel, &errors, assetPath.data(), static_cast<uint32_t>(assetPath.length()), &opts);

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
		}

		ENSURE(false);
	}

	Handle<Model> modelHandle = Resource::Allocate<Model>();

	std::vector<Resource::Vertex> vertices            = {};
	std::vector<uint32_t> indices                     = {};
	std::vector<Resource::MeshInstance> meshInstances = {};

	if (!ResolveModel(gltfModel, vertices, indices, meshInstances))
	{
		PRINT_ERROR("Failed resolving model from '{}'", assetPath);
	}
	else
	{
		Resource::CreateModel(modelHandle, std::move(vertices), std::move(indices), std::move(meshInstances));
	}

	tg3_model_free(&gltfModel);
	tg3_error_stack_free(&errors);

	return modelHandle;
}
