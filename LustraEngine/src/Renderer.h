#pragma once

#include "BindlessDescriptorPool.h"
#include "Buffer.h"
#include "LustraGLM.h"
#include "LustraVulkan.h"
#include "Model.h"
#include "Texture.h"

namespace Renderer
{
	struct InstanceData
	{
		glm::mat4 transform;
	};

	struct FrameConstants
	{
		glm::mat4 view;
		glm::mat4 proj;
	};

	struct FrameResources
	{
		vk::CommandPool commandPool          = nullptr;
		vk::CommandBuffer commandBuffer      = nullptr; // Will be destroyed with command pool
		vk::Semaphore imageAcquiredSemaphore = nullptr; // Binary semaphore

		// TODO: Find a better way to use this storage buffer without relying on BAR to be fully supported.
		AllocatedBuffer instanceTransformBuffer;
		AllocatedBuffer frameConstantsBuffer;

		// This is a set containing data that is constant accross the frame and can be used really anywhere.
		// Example: camera and instance transform bindings.
		vk::DescriptorSet agnosticConstantsSet;
	};

	struct ModelInstance
	{
		Handle<Resource::Model> modelHandle = nullhandle;
		glm::mat4 worldMatrix               = glm::mat4(1.0f);
	};

	constexpr uint32_t gMaxFramesInFlight = 2u;

	constexpr uint32_t gMaxMeshes    = 4096u;
	constexpr uint32_t gMaxMaterials = 1024u;

	inline Handle<Resource::Texture2D> gSceneDepth;
	inline std::array<FrameResources, gMaxFramesInFlight> gFramesInFlight = {};

	// Initialized at startup and has the same lifetime of the renderer itself.
	inline vk::DescriptorPool gStaticDescriptorPool;

	inline vk::PipelineLayout gHelloTrianglePipelineLayout;
	inline vk::Pipeline gHelloTrianglePipeline;

	inline vk::PipelineLayout gModelTestPipelineLayout;
	inline vk::Pipeline gModelTestPipeline;
	inline vk::DescriptorSetLayout gPerFrameDescLayout;

	inline vk::Semaphore gTimelineSemaphore;

	inline uint32_t gFrameIndex = 0u;

	inline BindlessDescriptorPool gBindlessPool;
	inline AllocatedBuffer gMaterialStorageBuffer;

	void Setup();
	void Destroy();

	void Render();
} // namespace Renderer
