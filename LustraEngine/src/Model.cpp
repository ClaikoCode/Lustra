#include "Model.h"

#include "Buffer.h"
#include "LustraLib/Assert.h"
#include "LustraVulkan.h"
#include "Resource.h"

namespace Resource
{
	void CreateModel(
	    Handle<Model> modelHandle,
	    std::vector<Vertex> vertices,
	    std::vector<uint32_t> indices,
	    std::vector<MeshInstance> instances
	)
	{
		Model* model = Resource::Get(modelHandle);
		ENSURE(model != nullptr);

		GPUMesh& gpuMesh = model->geomSource.gpuMesh;

		// Upload vertex data.
		gpuMesh.vertexBuffer = CreateBufferFromCPUData(
		    (void*)vertices.data(),
		    sizeof(Vertex) * vertices.size(),
		    vk::BufferUsageFlagBits::eVertexBuffer,
		    VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
		);

		// Upload index data.
		gpuMesh.indexBuffer = CreateBufferFromCPUData(
		    (void*)indices.data(),
		    sizeof(uint32_t) * indices.size(),
		    vk::BufferUsageFlagBits::eIndexBuffer,
		    VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
		);

		// Save CPU side data.
		model->geomSource.vertices = std::move(vertices);
		model->geomSource.indices  = std::move(indices);
		model->instances           = std::move(instances);
	}

	void DestroyModel(Handle<Model> modelHandle)
	{
		Model* model = Resource::Get(modelHandle);
		ENSURE(model != nullptr);

		DestroyBuffer(model->geomSource.gpuMesh.vertexBuffer);
		DestroyBuffer(model->geomSource.gpuMesh.indexBuffer);
	}
} // namespace Resource
