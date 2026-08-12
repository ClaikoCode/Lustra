#include "Renderer.h"

#include "AssetManager.h"
#include "AssetRegistry.h"
#include "Graphics.h"
#include "GraphicsUtils.h"
#include "LustraLib/Assert.h"
#include "Model.h"
#include "ModelImporter.h"
#include "Sampler.h"
#include "Shader.h"
#include "ShaderImporter.h"
#include "TextureImporter.h"

#include <array>
#include <chrono>

namespace fs = std::filesystem;
using namespace std::chrono_literals;
namespace chr = std::chrono;

// Has to be in nanoseconds.
// This is very helpful to fall back on when the GPU hangs.
static constexpr uint64_t kMaxSignalWait = chr::nanoseconds(5s).count();

namespace
{
	vk::PipelineShaderStageCreateInfo CreateShaderStageInfo(AssetID id)
	{
		const auto& shaderMeta         = AssetManager::GetMetadataFromID<Metadata::Shader>(id);
		const Resource::Shader* shader = Resource::Get(AssetRegistry::Resolve<Resource::Shader>(id));

		vk::ShaderStageFlagBits shaderStage;
		switch (shaderMeta.shaderType)
		{
			case ShaderTypeVS:
				shaderStage = vk::ShaderStageFlagBits::eVertex;
				break;
			case ShaderTypeFS:
				shaderStage = vk::ShaderStageFlagBits::eFragment;
				break;

			default:
				CHECK_UNREACHABLE();
				PRINT_WARNING("No valid shader type was found. Binding to all shader stages.");
				shaderStage = vk::ShaderStageFlagBits::eAll;
				break;
		}

		vk::PipelineShaderStageCreateInfo info = {
		    .stage  = shaderStage,
		    .module = shader->module,
		    .pName  = shaderMeta.entryPoint.c_str(), // Safe because shaderMeta lifetime is not in this scope.
		};

		return info;
	}

	vk::PipelineVertexInputStateCreateInfo CreateVertexInputStateDefault()
	{
		// Describe how vertices are going to be bound.
		static const vk::VertexInputBindingDescription bindingDesc = {
		    .binding = 0, .stride = sizeof(Resource::Vertex), .inputRate = vk::VertexInputRate::eVertex
		};

		static std::array<vk::VertexInputAttributeDescription, 5> attrs{};
		attrs[0] = {0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Resource::Vertex, pos)};        // vec3
		attrs[1] = {1, 0, vk::Format::eR32G32Sfloat, offsetof(Resource::Vertex, uv)};            // vec2
		attrs[2] = {2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Resource::Vertex, normal)};     // vec3
		attrs[3] = {3, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Resource::Vertex, tangent)}; // vec4
		attrs[4] = {4, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Resource::Vertex, col)};     // vec4

		// Describe how vertices are layed out in memory and in what primitives they describe.
		vk::PipelineVertexInputStateCreateInfo vertInputInfo = {};
		vertInputInfo.setVertexBindingDescriptions(bindingDesc);
		vertInputInfo.setVertexAttributeDescriptions(attrs);

		return vertInputInfo;
	}

	bool sShouldRecreateSwapchain    = false;
	bool sShouldUpdateMaterialBuffer = true;
	bool sShouldUpdateSamplers       = true;
	bool sShouldUpdateTextures       = true;
	uint64_t sNextSignalValue        = Renderer::gMaxFramesInFlight + 1;

} // namespace

namespace Renderer
{
	void CreateBindlessResources(BindlessResources& bindlessResources)
	{
		bindlessResources.samplerCache.Initialize();

		bindlessResources.materialStorageBuffer = CreateBuffer(
		    "Bindless Resources/Materials",
		    sizeof(Resource::GPUMaterial) * BindlessResources::kMaxMaterials,
		    vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
		    VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
		);

		// Create pool
		{
			std::array<vk::DescriptorPoolSize, BindlessResources::BindSlotCount> sizes = {};

			sizes[BindlessResources::BindSlotMaterials] = {
			    .type            = vk::DescriptorType::eStorageBuffer,
			    .descriptorCount = 1,
			};

			sizes[BindlessResources::BindSlotSamplers] = {
			    .type            = vk::DescriptorType::eSampler,
			    .descriptorCount = BindlessResources::kMaxSamplers,
			};

			sizes[BindlessResources::BindSlotTextures] = {
			    .type            = vk::DescriptorType::eSampledImage,
			    .descriptorCount = BindlessResources::kMaxTextureDescs,
			};

			vk::DescriptorPoolCreateInfo poolInfo = {
			    .maxSets = 1,
			};
			poolInfo.setPoolSizes(sizes);

			bindlessResources.descriptorPool =
			    AssertVk(Graphics::gVkDevice.createDescriptorPool(poolInfo, Graphics::gAllocationCallbacks));

			NameVk(Graphics::gVkDevice, bindlessResources.descriptorPool, "Bindless Pool");
		}

		// Desc set layout
		{
			std::array<vk::DescriptorSetLayoutBinding, BindlessResources::BindSlotCount> bindings = {};

			bindings[BindlessResources::BindSlotMaterials] = {
			    .binding         = BindlessResources::BindSlotMaterials,
			    .descriptorType  = vk::DescriptorType::eStorageBuffer,
			    .descriptorCount = 1,
			    .stageFlags      = vk::ShaderStageFlagBits::eFragment,
			};

			bindings[BindlessResources::BindSlotSamplers] = {
			    .binding         = BindlessResources::BindSlotSamplers,
			    .descriptorType  = vk::DescriptorType::eSampler,
			    .descriptorCount = BindlessResources::kMaxSamplers,
			    .stageFlags      = vk::ShaderStageFlagBits::eFragment,
			};

			bindings[BindlessResources::BindSlotTextures] = {
			    .binding         = BindlessResources::BindSlotTextures,
			    .descriptorType  = vk::DescriptorType::eSampledImage,
			    .descriptorCount = BindlessResources::kMaxTextureDescs,
			    .stageFlags      = vk::ShaderStageFlagBits::eFragment,
			};

			std::array<vk::DescriptorBindingFlags, BindlessResources::BindSlotCount> flags = {};

			// Allow for empty descriptors.
			flags[BindlessResources::BindSlotTextures] = vk::DescriptorBindingFlagBits::ePartiallyBound;
			flags[BindlessResources::BindSlotSamplers] = vk::DescriptorBindingFlagBits::ePartiallyBound;

			vk::DescriptorSetLayoutBindingFlagsCreateInfo flagInfo = {};
			flagInfo.setBindingFlags(flags);

			vk::DescriptorSetLayoutCreateInfo descLayoutInfo = {.pNext = &flagInfo};
			descLayoutInfo.setBindings(bindings);

			bindlessResources.layout =
			    AssertVk(Graphics::gVkDevice.createDescriptorSetLayout(descLayoutInfo, Graphics::gAllocationCallbacks));

			NameVk(Graphics::gVkDevice, bindlessResources.layout, "Bindless Layout");
		}

		// Allocate
		{
			vk::DescriptorSetAllocateInfo allocInfo = {
			    .descriptorPool = bindlessResources.descriptorPool,
			};
			allocInfo.setSetLayouts(bindlessResources.layout);

			bindlessResources.set = AssertVk(Graphics::gVkDevice.allocateDescriptorSets(allocInfo))[0];

			NameVk(Graphics::gVkDevice, bindlessResources.set, "Bindless Set");
		}

		// Write material buffer descriptor
		{
			vk::DescriptorBufferInfo buffInfo = {
			    .buffer = bindlessResources.materialStorageBuffer.buffer,
			    .offset = 0,
			    .range  = vk::WholeSize,
			};

			vk::WriteDescriptorSet write = {
			    .dstSet          = bindlessResources.set,
			    .dstBinding      = BindlessResources::BindSlotMaterials,
			    .dstArrayElement = 0,
			    .descriptorCount = 1,
			    .descriptorType  = vk::DescriptorType::eStorageBuffer,
			    .pBufferInfo     = &buffInfo,
			};

			Graphics::gVkDevice.updateDescriptorSets(write, nullptr);
		}
	}

	void DestroyBindlessResources(BindlessResources& bindlessResources)
	{
		Graphics::gVkDevice.destroy(bindlessResources.descriptorPool, Graphics::gAllocationCallbacks);
		Graphics::gVkDevice.destroy(bindlessResources.layout, Graphics::gAllocationCallbacks);

		DestroyBuffer(bindlessResources.materialStorageBuffer);

		bindlessResources.samplerCache.Destroy();
	}

	void UpdateMaterialBuffer(BindlessResources& bindlessResources)
	{
		const std::vector<Handle<Resource::Material>> materialHandles = Resource::GetAliveHandles<Resource::Material>();

		std::vector<Resource::GPUMaterial> gpuMats(BindlessResources::kMaxMaterials);
		for (auto matHandle : materialHandles)
		{
			const Resource::Material& mat = Resource::GetRef(matHandle);

			ENSURE(matHandle.index < gpuMats.size());
			gpuMats[matHandle.index] = Resource::FillGPUMaterialStruct(mat);
		}

		UploadData(
		    gpuMats.data(), gpuMats.size() * sizeof(Resource::GPUMaterial), bindlessResources.materialStorageBuffer
		);

		PRINT_DEBUG("Updated material buffer.");
	}

	void UpdateTextures(BindlessResources& bindlessResources)
	{
		const std::vector<Handle<Resource::Texture2D>> textureHandles =
		    Resource::GetAliveHandles<Resource::Texture2D>();

		Handle<Resource::Texture2D> fallbackTex = Resource::GetMissingTexture();
		vk::DescriptorImageInfo fallback        = {
		    .sampler     = nullptr,
		    .imageView   = Resource::GetRef(fallbackTex).view,
		    .imageLayout = vk::ImageLayout::eReadOnlyOptimal,
		};

		std::vector<vk::DescriptorImageInfo> texInfo(BindlessResources::kMaxTextureDescs, fallback);
		for (auto texHandle : textureHandles)
		{
			const Resource::Texture2D& tex = Resource::GetRef(texHandle);

			// If it isnt a samplable image, skip this texture.
			if (!(tex.desc.usage & vk::ImageUsageFlagBits::eSampled))
			{
				continue;
			}

			ENSURE(texHandle.index < texInfo.size());
			texInfo[texHandle.index] = {
			    .sampler     = nullptr,
			    .imageView   = tex.view,
			    .imageLayout = vk::ImageLayout::eReadOnlyOptimal,
			};
		}

		vk::WriteDescriptorSet write = {
		    .dstSet          = bindlessResources.set,
		    .dstBinding      = BindlessResources::BindSlotTextures,
		    .dstArrayElement = 0,
		    .descriptorType  = vk::DescriptorType::eSampledImage,
		};
		write.setImageInfo(texInfo);

		Graphics::gVkDevice.updateDescriptorSets(write, nullptr);
	}

	void UpdateSamplers(BindlessResources& bindlessResources)
	{
		const std::vector<Handle<Resource::Sampler2D>> samplerHandles =
		    Resource::GetAliveHandles<Resource::Sampler2D>();

		Handle<Resource::Sampler2D> fallbackSampler =
		    bindlessResources.samplerCache.GetDefaultSampler(DefaultSamplerLinearRepeat);
		vk::DescriptorImageInfo fallback = {.sampler = Resource::GetRef(fallbackSampler).sampler};

		std::vector<vk::DescriptorImageInfo> samplerInfos(BindlessResources::kMaxSamplers, fallback);
		for (auto samplerHandle : samplerHandles)
		{
			const Resource::Sampler2D sampler = Resource::GetRef(samplerHandle);

			ENSURE(samplerHandle.index < samplerInfos.size());
			samplerInfos[samplerHandle.index] = {
			    .sampler = sampler.sampler,
			};
		}

		vk::WriteDescriptorSet write = {
		    .dstSet          = bindlessResources.set,
		    .dstBinding      = BindlessResources::BindSlotSamplers,
		    .dstArrayElement = 0,
		    .descriptorType  = vk::DescriptorType::eSampler,
		};
		write.setImageInfo(samplerInfos);

		Graphics::gVkDevice.updateDescriptorSets(write, nullptr);
	}
} // namespace Renderer

namespace Renderer
{
	void Setup()
	{
		CreateBindlessResources(gBindlessResources);

		// Scene depth creation
		{
			Resource::TextureDesc2D depthDesc = Resource::CreateDepthDesc(
			    Graphics::gSwapchain.width, Graphics::gSwapchain.height, vk::Format::eD32Sfloat
			);

			gSceneDepth = Resource::Allocate<Resource::Texture2D>();
			Resource::CreateDepthTexture("Scene Depth", gSceneDepth, depthDesc);
		}

		// Per frame resources
		{
			for (uint32_t i = 0; i < gFramesInFlight.size(); i++)
			{
				FrameResources& frameResources = gFramesInFlight[i];

				const vk::CommandPoolCreateInfo commandPoolInfo = {.queueFamilyIndex = Graphics::graphicsQueue.index};
				frameResources.commandPool =
				    AssertVk(Graphics::gVkDevice.createCommandPool(commandPoolInfo, Graphics::gAllocationCallbacks));
				NameVk(
				    Graphics::gVkDevice, frameResources.commandPool, std::format("Frame Resources/Command Pool[{}]", i)
				);

				const vk::CommandBufferAllocateInfo commandAllocInfo = {
				    .commandPool        = frameResources.commandPool,
				    .level              = vk::CommandBufferLevel::ePrimary,
				    .commandBufferCount = 1
				};

				frameResources.commandBuffer =
				    AssertVk(Graphics::gVkDevice.allocateCommandBuffers(commandAllocInfo))[0];
				NameVk(
				    Graphics::gVkDevice,
				    frameResources.commandBuffer,
				    std::format("Frame Resources/Command Buffer[{}]", i)
				);

				frameResources.frameConstantsBuffer = CreateBuffer(
				    std::format("Frame Resources/Constants[{}]", i),
				    sizeof(FrameConstants),
				    vk::BufferUsageFlagBits::eUniformBuffer,
				    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
				);

				frameResources.instanceTransformBuffer = CreateBuffer(
				    std::format("Frame Resources/Instance Transforms[{}]", i),
				    gMaxMeshes * sizeof(InstanceData),
				    vk::BufferUsageFlagBits::eStorageBuffer,
				    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
				);
			}
		}

		// Descriptor pool
		{
			const std::array<vk::DescriptorPoolSize, 2> poolSizes{
			    vk::DescriptorPoolSize{
			        .type            = vk::DescriptorType::eStorageBuffer,
			        .descriptorCount = gMaxFramesInFlight,
			    },
			    vk::DescriptorPoolSize{
			        .type            = vk::DescriptorType::eUniformBuffer,
			        .descriptorCount = gMaxFramesInFlight,
			    }
			};

			vk::DescriptorPoolCreateInfo poolInfo = {};
			poolInfo.setPoolSizes(poolSizes);
			poolInfo.setMaxSets(gMaxFramesInFlight); // One set per frame

			gStaticDescriptorPool =
			    AssertVk(Graphics::gVkDevice.createDescriptorPool(poolInfo, Graphics::gAllocationCallbacks));

			NameVk(Graphics::gVkDevice, gStaticDescriptorPool, "Static Descriptor Pool");
		}

		// Graphics pipeline
		{
			vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {.setLayoutCount = 0, .pushConstantRangeCount = 0};

			gHelloTrianglePipelineLayout =
			    AssertVk(Graphics::gVkDevice.createPipelineLayout(pipelineLayoutInfo, Graphics::gAllocationCallbacks));

			std::array shaderStages = {
			    ::CreateShaderStageInfo(AssetKeyShaderVSTest), ::CreateShaderStageInfo(AssetKeyShaderFSTest)
			};

			vk::PipelineVertexInputStateCreateInfo vertInputInfo = ::CreateVertexInputStateDefault();

			const vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {
			    .topology = vk::PrimitiveTopology::eTriangleList
			};

			// Define how depth values should be handled.
			const vk::PipelineDepthStencilStateCreateInfo depthStencilInfo = {
			    .depthTestEnable   = vk::True,
			    .depthWriteEnable  = vk::True,
			    .depthCompareOp    = vk::CompareOp::eLess,
			    .stencilTestEnable = vk::False
			};

			// Viewport will be bound dynamically, so pointers are set to null.
			const vk::PipelineViewportStateCreateInfo viewportInfo = {
			    .viewportCount = 1,
			    .pViewports    = nullptr,
			    .scissorCount  = 1,
			    .pScissors     = nullptr,
			};

			// Describe how the previously defined primitives should be treated.
			const vk::PipelineRasterizationStateCreateInfo rasterInfo = {
			    .polygonMode = vk::PolygonMode::eFill,
			    .cullMode    = vk::CullModeFlagBits::eBack,
			    .frontFace   = vk::FrontFace::eCounterClockwise,
			    .lineWidth   = 1.0f,
			};

			// Multisampling information where a single sample is equivalent to no multi sampling.
			const vk::PipelineMultisampleStateCreateInfo multiSampleInfo = {
			    .rasterizationSamples = vk::SampleCountFlagBits::e1
			};

			// Tell vulkan how color should be written. What should be blended and which channels should be used.
			const vk::PipelineColorBlendAttachmentState attachState = {
			    .blendEnable = vk::False, .colorWriteMask = vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags
			};

			// Tell how blending should occurr when writing.
			const vk::PipelineColorBlendStateCreateInfo blendInfo = {
			    .attachmentCount = 1, .pAttachments = &attachState
			};

			// Describe the dynamic binding of viewport and scissor info.
			const std::array dynamicState = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
			const vk::PipelineDynamicStateCreateInfo dynamicStateInfo = {
			    .dynamicStateCount = dynamicState.size(), .pDynamicStates = dynamicState.data()
			};

			// Dynamic render EXT
			const vk::PipelineRenderingCreateInfo renderInfo = {
			    .colorAttachmentCount    = 1,
			    .pColorAttachmentFormats = &Graphics::gTargetSurfaceFormat.format,
			    .depthAttachmentFormat   = Graphics::gTargetDepthFormat
			};

			// Bring it all together to create the actual pipeline object.
			vk::GraphicsPipelineCreateInfo pipelineInfo = {
			    .pNext               = &renderInfo,
			    .stageCount          = shaderStages.size(),
			    .pStages             = shaderStages.data(),
			    .pVertexInputState   = &vertInputInfo,
			    .pInputAssemblyState = &inputAssemblyInfo,
			    .pViewportState      = &viewportInfo,
			    .pRasterizationState = &rasterInfo,
			    .pMultisampleState   = &multiSampleInfo,
			    .pDepthStencilState  = &depthStencilInfo,
			    .pColorBlendState    = &blendInfo,
			    .pDynamicState       = &dynamicStateInfo,
			    .layout              = gHelloTrianglePipelineLayout,
			    .renderPass          = VK_NULL_HANDLE
			};

			gHelloTrianglePipeline = AssertVk(
			    Graphics::gVkDevice.createGraphicsPipeline(nullptr, pipelineInfo, Graphics::gAllocationCallbacks)
			);

			// This has to equal the bindings on the shader side.
			std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
			    // Frame constants
			    vk::DescriptorSetLayoutBinding{
			        .binding         = 0,
			        .descriptorType  = vk::DescriptorType::eUniformBuffer,
			        .descriptorCount = 1,
			        .stageFlags      = vk::ShaderStageFlagBits::eVertex,
			    },

			    // Transforms
			    vk::DescriptorSetLayoutBinding{
			        .binding         = 1,
			        .descriptorType  = vk::DescriptorType::eStorageBuffer,
			        .descriptorCount = 1,
			        .stageFlags      = vk::ShaderStageFlagBits::eVertex,
			    },
			};

			vk::DescriptorSetLayoutCreateInfo descSetLayoutInfo = {};
			descSetLayoutInfo.setBindings(bindings);

			gPerFrameDescLayout = AssertVk(
			    Graphics::gVkDevice.createDescriptorSetLayout(descSetLayoutInfo, Graphics::gAllocationCallbacks)
			);

			// Allocate and write descriptor sets
			{
				std::array<vk::DescriptorSetLayout, gMaxFramesInFlight> layouts = {};
				layouts.fill(gPerFrameDescLayout); // Same layout for each set to be allocated.

				vk::DescriptorSetAllocateInfo setAllocInfo = {};
				setAllocInfo.setDescriptorPool(gStaticDescriptorPool);
				setAllocInfo.setDescriptorSetCount(gMaxFramesInFlight);
				setAllocInfo.setSetLayouts(layouts);

				std::vector<vk::DescriptorSet> sets =
				    AssertVk(Graphics::gVkDevice.allocateDescriptorSets(setAllocInfo));

				// Bind descriptor sets with buffer descriptors per frame.
				for (uint32_t i = 0; i < gMaxFramesInFlight; i++)
				{
					FrameResources& frame      = gFramesInFlight[i];
					frame.agnosticConstantsSet = sets[i];

					const vk::DescriptorBufferInfo uboInfo = {
					    .buffer = frame.frameConstantsBuffer.buffer, .offset = 0, .range = sizeof(FrameConstants)
					};

					const vk::DescriptorBufferInfo ssboInfo = {
					    .buffer = frame.instanceTransformBuffer.buffer, .offset = 0, .range = vk::WholeSize
					};

					const std::array<vk::WriteDescriptorSet, 2> writes = {
					    vk::WriteDescriptorSet{
					        .dstSet          = frame.agnosticConstantsSet,
					        .dstBinding      = 0,
					        .dstArrayElement = 0,
					        .descriptorCount = 1,
					        .descriptorType  = vk::DescriptorType::eUniformBuffer,
					        .pBufferInfo     = &uboInfo,
					    },

					    vk::WriteDescriptorSet{
					        .dstSet          = frame.agnosticConstantsSet,
					        .dstBinding      = 1,
					        .dstArrayElement = 0,
					        .descriptorCount = 1,
					        .descriptorType  = vk::DescriptorType::eStorageBuffer,
					        .pBufferInfo     = &ssboInfo,
					    },
					};

					Graphics::gVkDevice.updateDescriptorSets(writes, {});
				}
			}

			const std::array pcRanges = {
			    vk::PushConstantRange{
			        .stageFlags = vk::ShaderStageFlagBits::eVertex,
			        .offset     = 0,
			        .size       = sizeof(uint32_t) // transform index
			    },

			    vk::PushConstantRange{
			        .stageFlags = vk::ShaderStageFlagBits::eFragment,
			        .offset     = sizeof(uint32_t),
			        .size       = sizeof(uint32_t) // material index
			    },
			};

			// This order must match the set indices inside shaders layout(set = i).
			const std::array setLayouts = {gBindlessResources.layout, gPerFrameDescLayout};

			pipelineLayoutInfo = {};
			pipelineLayoutInfo.setSetLayouts(setLayouts);
			pipelineLayoutInfo.setPushConstantRanges(pcRanges);

			gModelTestPipelineLayout =
			    AssertVk(Graphics::gVkDevice.createPipelineLayout(pipelineLayoutInfo, Graphics::gAllocationCallbacks));

			shaderStages = {
			    ::CreateShaderStageInfo(AssetKeyShaderFSModelTest), ::CreateShaderStageInfo(AssetKeyShaderVSModelTest)
			};

			// Keep all other pipeline defaults but these.
			pipelineInfo.setStages(shaderStages);
			pipelineInfo.setLayout(gModelTestPipelineLayout);

			gModelTestPipeline = AssertVk(
			    Graphics::gVkDevice.createGraphicsPipeline(nullptr, pipelineInfo, Graphics::gAllocationCallbacks)
			);
		}

		// Sync resources
		{
			// Timeline semaphore.
			{
				const vk::SemaphoreTypeCreateInfo semaphoreTypeInfo = {
				    .semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = gMaxFramesInFlight
				};

				const vk::SemaphoreCreateInfo semaphoreInfo = {.pNext = &semaphoreTypeInfo};

				gTimelineSemaphore =
				    AssertVk(Graphics::gVkDevice.createSemaphore(semaphoreInfo, Graphics::gAllocationCallbacks));

				NameVk(Graphics::gVkDevice, gTimelineSemaphore, "Timeline Semaphore");
			}

			// Per frame image-acquire semaphores.
			uint32_t count = 0;
			for (FrameResources& frameResource : gFramesInFlight)
			{
				const vk::SemaphoreCreateInfo semaphoreInfo = {};

				frameResource.imageAcquiredSemaphore =
				    AssertVk(Graphics::gVkDevice.createSemaphore(semaphoreInfo, Graphics::gAllocationCallbacks));

				NameVk(
				    Graphics::gVkDevice,
				    frameResource.imageAcquiredSemaphore,
				    std::format("Frame Resources/Timeline Semaphore[{}]", count++)
				);
			}
		}

		// TODO: REMOVE LATER
		{
			AssetRegistry::Resolve<Resource::Model>(AssetKeySponza);
		}

		PRINT_DEBUG("Renderer successfully set up.");

	} // namespace Renderer

	void Destroy()
	{
		// Release all internal resources. Resource pools will clear themselves elsewere.
		Resource::Release(gSceneDepth);

		DestroyBindlessResources(gBindlessResources);

		// Destroy all other GPU resources.
		Graphics::gVkDevice.destroy(gHelloTrianglePipeline, Graphics::gAllocationCallbacks);
		Graphics::gVkDevice.destroy(gHelloTrianglePipelineLayout, Graphics::gAllocationCallbacks);

		Graphics::gVkDevice.destroy(gModelTestPipeline, Graphics::gAllocationCallbacks);
		Graphics::gVkDevice.destroy(gModelTestPipelineLayout, Graphics::gAllocationCallbacks);
		Graphics::gVkDevice.destroy(gPerFrameDescLayout, Graphics::gAllocationCallbacks);

		// Also takes care of all allocated descriptor sets.
		Graphics::gVkDevice.destroy(gStaticDescriptorPool);

		Graphics::gVkDevice.destroy(gTimelineSemaphore, Graphics::gAllocationCallbacks);

		for (FrameResources& frameResources : gFramesInFlight)
		{
			Graphics::gVkDevice.destroy(frameResources.commandPool, Graphics::gAllocationCallbacks);
			Graphics::gVkDevice.destroy(frameResources.imageAcquiredSemaphore, Graphics::gAllocationCallbacks);

			DestroyBuffer(frameResources.frameConstantsBuffer);
			DestroyBuffer(frameResources.instanceTransformBuffer);
		}
	}

	void Render()
	{
		// Process draw calls
		// TODO: Move to some Update() function.
		std::vector<ModelInstance> modelInstances = {};
		{
			Handle<Resource::Model> modelTest = AssetRegistry::Resolve<Resource::Model>(AssetKeyModelTest);
			modelInstances.push_back({
			    .modelHandle = modelTest,
			    .worldMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)),
			});

			Handle<Resource::Model> sponza = AssetRegistry::Resolve<Resource::Model>(AssetKeySponza);
			modelInstances.push_back({
			    .modelHandle = sponza,
			    .worldMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(4.0f)),
			});
		}

		const bool shouldUpdateBindlessResources =
		    sShouldUpdateMaterialBuffer || sShouldUpdateSamplers || sShouldUpdateTextures;

		if (shouldUpdateBindlessResources)
		{
			// Wait before uploading to ensure any frames in flights are guaranteed to be done using their resources.
			Graphics::WaitForDevice();
		}

		if (sShouldUpdateMaterialBuffer)
		{
			UpdateMaterialBuffer(gBindlessResources);
			sShouldUpdateMaterialBuffer = false;
		}

		if (sShouldUpdateSamplers)
		{
			UpdateSamplers(gBindlessResources);
			sShouldUpdateSamplers = false;
		}

		if (sShouldUpdateTextures)
		{
			UpdateTextures(gBindlessResources);
			sShouldUpdateTextures = false;
		}

		if (sShouldRecreateSwapchain)
		{
			PRINT_LOG("Recreating swapchain...");

			Graphics::WaitForDevice();

			ENSURE(Graphics::gWindowPtr != nullptr);

			Graphics::gSwapchain.Destroy();
			Graphics::CreateSwapchain(*Graphics::gWindowPtr);

			// TODO: Make a better way of tracking resources that have any properties bound to the swapchain and make
			// sure to recreate them along with the swapchain.
			Resource::ResizeTexture(gSceneDepth, Graphics::gSwapchain.width, Graphics::gSwapchain.height);

			sShouldRecreateSwapchain = false;

			PRINT_LOG("Done recreating swapchain!");
		}

		const uint32_t frameResourceIndex = gFrameIndex++ % gMaxFramesInFlight;
		const uint64_t signalValue        = sNextSignalValue++;
		const uint64_t waitValue          = signalValue - gMaxFramesInFlight;

		const vk::SemaphoreWaitInfo waitInfo = {
		    .semaphoreCount = 1, .pSemaphores = &gTimelineSemaphore, .pValues = &waitValue
		};

		// Force the CPU to wait for the GPU to be done with last session of the same resources that are going to be
		// worked on THIS frame.
		AssertVk(Graphics::gVkDevice.waitSemaphores(waitInfo, kMaxSignalWait));

		// GPU has signaled that resources are free to use so its time to fetch them.
		const FrameResources& frameResources = gFramesInFlight[frameResourceIndex];

		static const auto startTime = std::chrono::steady_clock::now();
		const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();

		// Write frame CPU data to GPU buffers.
		{
			// Write transform data.
			{
				InstanceData* const instanceDataArray =
				    reinterpret_cast<InstanceData*>(frameResources.instanceTransformBuffer.info.pMappedData);
				uint32_t transformIndex = 0u;
				for (const ModelInstance& modelInstance : modelInstances)
				{
					const Resource::Model* model = Resource::Get(modelInstance.modelHandle);
					for (const Resource::MeshInstance& meshInstance : model->instances)
					{
						instanceDataArray[transformIndex].transform =
						    modelInstance.worldMatrix * meshInstance.transform;

						// Increment for each mesh transform copied.
						transformIndex++;
					}
				}

				// Flush
				vmaFlushAllocation(
				    Graphics::gVmaAllocator, frameResources.instanceTransformBuffer.alloc, 0, VK_WHOLE_SIZE
				);
			}

			// Write frame constant data.
			{
				FrameConstants fc = {};

				const float angle  = t * glm::radians(45.0f); // 45 deg/sec orbit
				const float radius = 10.0f;                   // distance from the model

				// A camera sitting back on + Z, looking at the origin.
				const glm::vec3 eye = {
				    radius * std::sin(angle),
				    5.0f,
				    radius * std::cos(angle),
				};
				const glm::vec3 center = {0.0f, 0.0f, 0.0f};
				const glm::vec3 up     = {0.0f, 1.0f, 0.0f};
				fc.view                = glm::lookAt(eye, center, up);

				const float aspect =
				    static_cast<float>(Graphics::gSwapchain.width) / static_cast<float>(Graphics::gSwapchain.height);
				fc.proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);

				// Reverse Y since GLM assumes OpenGL standard which has NDC +Y as up when Vulkan assumes +Y as down.
				fc.proj[1][1] *= -1.0f;

				// Copy over the data.
				memcpy(frameResources.frameConstantsBuffer.info.pMappedData, &fc, sizeof(fc));

				// Flush
				vmaFlushAllocation(
				    Graphics::gVmaAllocator, frameResources.frameConstantsBuffer.alloc, 0, VK_WHOLE_SIZE
				);
			}
		}

		// Prep for recording new commands.
		AssertVk(Graphics::gVkDevice.resetCommandPool(frameResources.commandPool));

		// This semaphore will be used to check if the image we are going to be rendering to later is free to write to.
		const vk::Semaphore imageAcquireSemaphore = frameResources.imageAcquiredSemaphore;

		// Ask the device if it has a swapchain index it knows is going to be available for writes later and bind it
		// with the semaphore.
		// NOTE: This uses the C API because vulkan.hpp without exceptions (understandably) asserts an out of date
		// code as an error, stopping the program. Using the C API lets the renderer recover in those cases.
		ENSURE(VULKAN_HPP_DEFAULT_DISPATCHER.vkAcquireNextImageKHR != nullptr);
		uint32_t imageAcquiredIndex = 0u;
		const auto imageAcquireResultValue =
		    static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkAcquireNextImageKHR(
		        Graphics::gVkDevice,
		        Graphics::gSwapchain.swapchain,
		        kMaxSignalWait,
		        imageAcquireSemaphore,
		        VK_NULL_HANDLE,
		        &imageAcquiredIndex
		    ));

		if (imageAcquireResultValue == vk::Result::eErrorOutOfDateKHR)
		{
			sShouldRecreateSwapchain = true;
			PRINT_DEBUG(
			    "Image acquire resulted in '{}'. Asking for swapchain recreation and returning immediately.",
			    vk::to_string(imageAcquireResultValue)
			);
			return;
		}
		// Image is suboptimal and swapchain should be recreated but the frame can continue.
		else if (imageAcquireResultValue == vk::Result::eSuboptimalKHR)
		{
			sShouldRecreateSwapchain = true;
			PRINT_DEBUG(
			    "Image acquire resulted in '{}'. Asking for swapchain recreation next frame but continuing with the "
			    "current frame.",
			    vk::to_string(imageAcquireResultValue)
			);
		}
		else
		{
			// If not any of the plausible values above, assert it to check if its an error.
			AssertVk(imageAcquireResultValue);
		}

		const uint32_t imageIndex             = imageAcquiredIndex;
		const vk::CommandBuffer commandBuffer = frameResources.commandBuffer;

		// Render the frame.
		{
			const vk::Image swapchainImage    = Graphics::gSwapchain.images[imageIndex];
			const vk::ImageView swapchainView = Graphics::gSwapchain.views[imageIndex];

			const uint32_t targetWidth  = Graphics::gSwapchain.width;
			const uint32_t targetHeight = Graphics::gSwapchain.height;

			const vk::CommandBufferBeginInfo cmdBeginInfo = {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};

			AssertVk(commandBuffer.begin(cmdBeginInfo));

			const std::array layoutBarriers = {
			    vk::ImageMemoryBarrier2{
			        .srcStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			        .srcAccessMask = vk::AccessFlagBits2::eNone,
			        .dstStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			        .oldLayout     = vk::ImageLayout::eUndefined,
			        .newLayout     = vk::ImageLayout::eColorAttachmentOptimal,
			        .image         = swapchainImage,
			        .subresourceRange =
			            {
			                .aspectMask     = vk::ImageAspectFlagBits::eColor,
			                .baseMipLevel   = 0,
			                .levelCount     = 1,
			                .baseArrayLayer = 0,
			                .layerCount     = 1,
			            }

			    },
			    vk::ImageMemoryBarrier2{
			        .srcStageMask     = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
			        .srcAccessMask    = vk::AccessFlagBits2::eNone,
			        .dstStageMask     = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
			                            vk::PipelineStageFlagBits2::eLateFragmentTests,
			        .dstAccessMask    = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			        .oldLayout        = vk::ImageLayout::eUndefined,
			        .newLayout        = vk::ImageLayout::eDepthAttachmentOptimal,
			        .image            = *Resource::Get(gSceneDepth),
			        .subresourceRange = {
			            .aspectMask     = vk::ImageAspectFlagBits::eDepth,
			            .baseMipLevel   = 0,
			            .levelCount     = 1,
			            .baseArrayLayer = 0,
			            .layerCount     = 1,
			        }
			    }
			};

			const vk::DependencyInfo depInfo = {
			    .imageMemoryBarrierCount = layoutBarriers.size(), .pImageMemoryBarriers = layoutBarriers.data()
			};
			commandBuffer.pipelineBarrier2(depInfo);

			constexpr std::array clearColor                   = {0.0f, 0.0f, 0.0f, 1.0f};
			const vk::RenderingAttachmentInfo colorAttachInfo = {
			    .imageView   = swapchainView,
			    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			    .loadOp      = vk::AttachmentLoadOp::eClear,  // Clear image
			    .storeOp     = vk::AttachmentStoreOp::eStore, // Keep data for presentation
			    .clearValue  = {.color = {.float32 = clearColor}}
			};

			const vk::RenderingAttachmentInfo depthAttachInfo = {
			    .imageView   = Resource::Get(gSceneDepth)->view,
			    .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			    .loadOp      = vk::AttachmentLoadOp::eClear,
			    .storeOp     = vk::AttachmentStoreOp::eDontCare,
			    .clearValue  = {.depthStencil = {.depth = 1.0f, .stencil = 0}}
			};

			const vk::RenderingInfo renderingInfo = {
			    .renderArea =
			        {.offset = {.x = 0, .y = 0},
			         .extent = {.width = targetWidth, .height = targetHeight}}, // Dictates render area
			    .layerCount           = 1,
			    .colorAttachmentCount = 1,
			    .pColorAttachments    = &colorAttachInfo,
			    .pDepthAttachment     = &depthAttachInfo
			};

			// Begin dynamic rendering!!!!!!!!!!!!!!!!!!!!
			commandBuffer.beginRendering(renderingInfo);

			{
				const vk::Viewport viewport = {
				    .x        = 0,
				    .y        = 0,
				    .width    = static_cast<float>(targetWidth),
				    .height   = static_cast<float>(targetHeight),
				    .minDepth = 0.0f,
				    .maxDepth = 1.0f
				};
				commandBuffer.setViewport(0, 1, &viewport);

				const vk::Rect2D scissor = {
				    .offset = {.x = 0, .y = 0},
				    .extent = {.width = targetWidth, .height = targetHeight},
				};
				commandBuffer.setScissor(0, 1, &scissor);

				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, gModelTestPipeline);

				const vk::PipelineLayout currentLayout = gModelTestPipelineLayout;
				commandBuffer.bindDescriptorSets(
				    vk::PipelineBindPoint::eGraphics,
				    currentLayout,
				    0,
				    {gBindlessResources.set, frameResources.agnosticConstantsSet},
				    {}
				);

				uint32_t baseTransformIndex = 0;
				for (const ModelInstance& modelInstance : modelInstances)
				{
					const Resource::Model& model = *Resource::Get(modelInstance.modelHandle);

					// Bind geometry buffers per model.
					commandBuffer.bindVertexBuffers(0, model.geomSource.gpuMesh.vertexBuffer.buffer, {0});
					commandBuffer.bindIndexBuffer(
					    model.geomSource.gpuMesh.indexBuffer.buffer, 0, vk::IndexType::eUint32
					);

					// Go through all mesh instances.
					for (uint32_t instanceIndex = 0; instanceIndex < model.instances.size(); instanceIndex++)
					{
						const uint32_t transformIndex = baseTransformIndex + instanceIndex;
						// Push the transform index per mesh instance.
						commandBuffer.pushConstants<uint32_t>(
						    currentLayout, vk::ShaderStageFlagBits::eVertex, 0, transformIndex
						);

						const auto& subMeshes = model.instances[instanceIndex].subMeshes;
						for (const auto& submesh : subMeshes)
						{
							const Resource::PrimitiveRange& range = submesh.range;

							// Push material index per submesh.
							commandBuffer.pushConstants<uint32_t>(
							    currentLayout, vk::ShaderStageFlagBits::eFragment, sizeof(uint32_t), submesh.mat.index
							);

							commandBuffer.drawIndexed(range.indexCount, 1, range.startIndex, range.vertexOffset, 0);
						}
					}

					// Increase base index.
					baseTransformIndex += model.instances.size();
				}
			}

			commandBuffer.endRendering();

			// Transition from color attachement to presentation.
			const vk::ImageMemoryBarrier2 presentLayoutBarrier = {
			    .srcStageMask     = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			    .srcAccessMask    = vk::AccessFlagBits2::eColorAttachmentWrite,
			    .dstStageMask     = vk::PipelineStageFlagBits2::eNone,
			    .dstAccessMask    = vk::AccessFlagBits2::eNone,
			    .oldLayout        = vk::ImageLayout::eColorAttachmentOptimal,
			    .newLayout        = vk::ImageLayout::ePresentSrcKHR,
			    .image            = swapchainImage,
			    .subresourceRange = {
			        .aspectMask     = vk::ImageAspectFlagBits::eColor,
			        .baseMipLevel   = 0,
			        .levelCount     = 1,
			        .baseArrayLayer = 0,
			        .layerCount     = 1,
			    }
			};

			const vk::DependencyInfo presentDepInfo = {
			    .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &presentLayoutBarrier
			};
			commandBuffer.pipelineBarrier2(presentDepInfo);

			AssertVk(commandBuffer.end());
		}

		// Submit and Present
		{
			const vk::Semaphore renderingCompleteSemaphore = Graphics::gSwapchain.semaphores[imageIndex];
			const vk::SwapchainKHR swapchain               = Graphics::gSwapchain.swapchain;
			const vk::Queue graphicsQueue                  = Graphics::graphicsQueue.queue;

			// We have the index for which swapchain image we are going to write to but now we have to wait for the
			// image at that index to actually be available. This semaphore is tied to the swapchain image so as
			// soon as the image at the index that was given to prior can be written to, a signal is sent.
			// NOTE:
			// This is not a CPU wait. The GPU will run all commands submitted but will stop and wait for the
			// semaphore to be signaled at the point of trying to bind the image for color output in the pipeline.
			// Earlier pipeline stages are free to run.
			// TODO:
			// Work should be split into different command buffers because this wait waits on the FIRST
			// instance of color output attachement, even if the target is not the swapchain. This wait should only
			// be for the command buffer that has commands that will write to the swapchain, all other kind of work
			// should be separate to avoid unecessary GPU stalls.
			const vk::SemaphoreSubmitInfo imageAcquiredWaitInfo = {
			    .semaphore = imageAcquireSemaphore, .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
			};

			const std::array semaphoreSignals = {
			    // This binary semaphore is used by the presentation engine. The GPU will signal this semaphore once
			    // all GRAPHICS commmands has been completed, meaning the swapchain image is ready to be presented.
			    vk::SemaphoreSubmitInfo{
			        .semaphore = renderingCompleteSemaphore, .stageMask = vk::PipelineStageFlagBits2::eAllGraphics
			    },
			    // Once ALL commands are done (not only graphics), signal the timeline semaphore with this frames
			    // signal value to finally communicate that all resources associated with this frame has been completed.
			    vk::SemaphoreSubmitInfo{
			        .semaphore = gTimelineSemaphore,
			        .value     = signalValue,
			        .stageMask = vk::PipelineStageFlagBits2::eAllCommands
			    }
			};

			const vk::CommandBufferSubmitInfo cmdSubmitInfo = {.commandBuffer = commandBuffer};
			const vk::SubmitInfo2 submitInfo                = {
			    .waitSemaphoreInfoCount = 1,
			    // Semaphores to be waited on before executing command buffer.
			    .pWaitSemaphoreInfos      = &imageAcquiredWaitInfo,
			    .commandBufferInfoCount   = 1,
			    .pCommandBufferInfos      = &cmdSubmitInfo,
			    .signalSemaphoreInfoCount = semaphoreSignals.size(),
			    // Semaphores to signal once certain stages of the pipeline have been completed.
			    .pSignalSemaphoreInfos = semaphoreSignals.data()
			};

			// Submit all commands.
			AssertVk(graphicsQueue.submit2(1, &submitInfo, VK_NULL_HANDLE));

			const vk::PresentInfoKHR presentInfo = {
			    .waitSemaphoreCount = 1,
			    // Waits for the semaphore that will trigger once all graphcis rendering is complete.
			    .pWaitSemaphores = &renderingCompleteSemaphore,
			    .swapchainCount  = 1,
			    .pSwapchains     = &swapchain,
			    .pImageIndices   = &imageIndex,
			    .pResults        = nullptr
			};

			// Present the swapchain image.
			// NOTE: This uses the C API because vulkan.hpp without exceptions (understandably) asserts an out of
			// date code as an error, stopping the program. Using the C API lets the renderer recover in those
			// cases.
			ENSURE(VULKAN_HPP_DEFAULT_DISPATCHER.vkQueuePresentKHR != nullptr);
			const VkPresentInfoKHR& presentInfoC = presentInfo;
			const auto presentResult =
			    static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkQueuePresentKHR(graphicsQueue, &presentInfoC));

			// Because this is the last thing that is done this frame, both suboptimal and out of date are handled
			// NEXT frame.
			if (presentResult == vk::Result::eSuboptimalKHR || presentResult == vk::Result::eErrorOutOfDateKHR)
			{
				PRINT_DEBUG(
				    "Present resulted in '{}'. Swapchain will be asked to be recreated the coming frame.",
				    vk::to_string(presentResult)
				);
				sShouldRecreateSwapchain = true;
			}
			else
			{
				AssertVk(presentResult);
			}
		}
	}
} // namespace Renderer
