#include "Model.h"

#include "Buffer.h"
#include "LustraLib/Assert.h"
#include "LustraVulkan.h"
#include "Resource.h"

namespace Resource
{
	void CreateModel(
	    std::string_view name,
	    Handle<Model> modelHandle,
	    std::vector<Vertex> vertices,
	    std::vector<uint32_t> indices,
	    std::vector<MeshInstance> instances
	)
	{
		Model* model = Resource::Get(modelHandle);
		ENSURE(model != nullptr);

		model->name = std::string(name);

		GPUMesh& gpuMesh = model->geomSource.gpuMesh;

		// Upload vertex data.
		gpuMesh.vertexBuffer = CreateBufferFromCPUData(
		    std::format("{}/Vertex Buffer", model->name),
		    (void*)vertices.data(),
		    sizeof(Vertex) * vertices.size(),
		    vk::BufferUsageFlagBits::eVertexBuffer,
		    VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
		);

		// Upload index data.
		gpuMesh.indexBuffer = CreateBufferFromCPUData(
		    std::format("{}/Index Buffer", model->name),
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

		for (MeshInstance& meshInstance : model->instances)
		{
			for (SubMesh& subMesh : meshInstance.subMeshes)
			{
				Release(subMesh.mat);
			}
		}
	}
} // namespace Resource
