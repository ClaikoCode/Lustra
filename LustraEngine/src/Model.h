#pragma once

#include "Buffer.h"
#include "Handle.h"
#include "LustraGLM.h"
#include "LustraVulkan.h"
#include "Material.h"

#include <vector>

namespace Resource
{
	// TODO: Move to other relevant file.
	struct AABB
	{
		glm::vec3 min = {};
		glm::vec3 max = {};
	};

	struct Vertex
	{
		glm::vec3 pos     = {};                       // POSITION
		glm::vec2 uv      = {};                       // TEXCOORD_0
		glm::vec3 normal  = {};                       // NORMAL
		glm::vec4 tangent = {};                       // TANGENT
		glm::vec4 col     = {1.0f, 1.0f, 1.0f, 1.0f}; // COLOR_0 (white)
	};

	struct PrimitiveRange
	{
		uint32_t startIndex;
		uint32_t indexCount;
		int32_t vertexOffset;
	};

	struct SubMesh
	{
		PrimitiveRange range;
		Handle<Material> mat;
	};

	struct GPUMesh
	{
		AllocatedBuffer vertexBuffer;
		AllocatedBuffer indexBuffer;
	};

	struct GeometrySource
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		GPUMesh gpuMesh;
	};

	struct MeshInstance
	{
		std::vector<SubMesh> subMeshes; // These are the draw calls, a subset of the source geometry.
		glm::mat4 transform;
	};

	struct Model : ResourceTag
	{
		GeometrySource geomSource;
		std::vector<MeshInstance> instances; // Flattened scene graph from import.
	};

	// Allocates and uploads vertex and index buffers and also saves CPU data. The function calls std::move() on input
	// vectors internally.
	// NOTE: If input params are not intended to be used after calling this function, consider calling std::move() on
	// them.
	void CreateModel(
	    std::string_view name,
	    Handle<Model> modelHandle,
	    std::vector<Vertex> vertices,
	    std::vector<uint32_t> indices,
	    std::vector<MeshInstance> instances
	);

	void DestroyModel(Handle<Model> modelHandle);

} // namespace Resource
