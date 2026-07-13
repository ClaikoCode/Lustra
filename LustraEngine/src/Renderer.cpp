#include "Renderer.h"

#include "AssetManager.h"
#include "Graphics.h"
#include "GraphicsUtils.h"
#include "LustraLib/Assert.h"
#include "Shader.h"

#include <array>

namespace fs = std::filesystem;

static constexpr uint64_t kInfiniteWait = UINT64_MAX;

namespace
{
	vk::PipelineShaderStageCreateInfo CreateShaderStageInfo(AssetID id)
	{
		const auto& shaderMeta         = AssetManager::GetEntry(id).GetMetadata<Metadata::Shader>();
		const Resource::Shader* shader = AssetManager::GetHandle<Resource::Shader>(id).Get();

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
		    .stage = shaderStage, .module = shader->module, .pName = shaderMeta.entryPoint.c_str()
		};

		return info;
	}

	bool sShouldRecreateSwapchain = false;
	uint64_t sNextSignalValue     = Renderer::gMaxFramesInFlight + 1;

} // namespace

namespace Renderer
{
	void Setup()
	{
		// Scene depth creation
		{
			gSceneDepth = Resource::CreateDepthTexture(
			    Graphics::gSwapchain.width, Graphics::gSwapchain.height, vk::Format::eD32Sfloat
			);
		}

		// Shader objects
		{
			AssetManager::BuildShadersFromDatabase();
		}

		// Graphics pipeline
		{
			const vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {.setLayoutCount = 0, .pushConstantRangeCount = 0};

			gHelloTrianglePipelineLayout =
			    AssertVk(Graphics::gVkDevice.createPipelineLayout(pipelineLayoutInfo, Graphics::gAllocationCallbacks));

			const std::array shaderStages = {
			    ::CreateShaderStageInfo(AssetKeyShaderVSTest), ::CreateShaderStageInfo(AssetKeyShaderFSTest)
			};

			// Describe how vertices are layed out in memory and in what primitives they describe.
			const vk::PipelineVertexInputStateCreateInfo vertInputInfo       = {};
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
			    .blendEnable = vk::False, .colorWriteMask = vk::ColorComponentFlags()
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
			const vk::GraphicsPipelineCreateInfo pipelineInfo = {
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
			}

			// Per frame image-acquire semaphores.
			for (FrameResources& frameResource : gFramesInFlight)
			{
				const vk::SemaphoreCreateInfo semaphoreInfo = {};

				frameResource.imageAcquiredSemaphore =
				    AssertVk(Graphics::gVkDevice.createSemaphore(semaphoreInfo, Graphics::gAllocationCallbacks));
			}
		}

		// Command buffers
		{
			for (FrameResources& frameResources : gFramesInFlight)
			{
				const vk::CommandPoolCreateInfo commandPoolInfo = {.queueFamilyIndex = Graphics::graphicsQueue.index};
				frameResources.commandPool =
				    AssertVk(Graphics::gVkDevice.createCommandPool(commandPoolInfo, Graphics::gAllocationCallbacks));

				const vk::CommandBufferAllocateInfo commandAllocInfo = {
				    .commandPool        = frameResources.commandPool,
				    .level              = vk::CommandBufferLevel::ePrimary,
				    .commandBufferCount = 1
				};

				frameResources.commandBuffer =
				    AssertVk(Graphics::gVkDevice.allocateCommandBuffers(commandAllocInfo))[0];
			}
		}
	} // namespace Renderer

	void Destroy()
	{
		Resource::DestroyDepthTexture(gSceneDepth);
		gSceneDepth.Release();

		Graphics::gVkDevice.destroy(gHelloTrianglePipeline, Graphics::gAllocationCallbacks);
		Graphics::gVkDevice.destroy(gHelloTrianglePipelineLayout, Graphics::gAllocationCallbacks);
		Graphics::gVkDevice.destroy(gTimelineSemaphore, Graphics::gAllocationCallbacks);

		for (const FrameResources& frameResources : gFramesInFlight)
		{
			Graphics::gVkDevice.destroy(frameResources.commandPool, Graphics::gAllocationCallbacks);
			Graphics::gVkDevice.destroy(frameResources.imageAcquiredSemaphore, Graphics::gAllocationCallbacks);
		}
	}

	void Render()
	{
		// TODO: Move this to Graphics since that should be responsible for this type of check and handling.
		if (sShouldRecreateSwapchain)
		{
			ENSURE(Graphics::gWindowPtr != nullptr);

			PRINT_LOG("Recreating swapchain.");

			Graphics::gSwapchain.Destroy();
			Graphics::CreateSwapchain(*Graphics::gWindowPtr);

			sShouldRecreateSwapchain = false;
		}

		const uint32_t frameResourceIndex = gFrameIndex++ % gMaxFramesInFlight;
		const uint64_t signalValue        = sNextSignalValue++;
		const uint64_t waitValue          = signalValue - gMaxFramesInFlight;

		const vk::SemaphoreWaitInfo waitInfo = {
		    .semaphoreCount = 1, .pSemaphores = &gTimelineSemaphore, .pValues = &waitValue
		};

		// Force the CPU to wait for the GPU to be done with last session of the same resources that are going to be
		// worked on THIS frame.
		AssertVk(Graphics::gVkDevice.waitSemaphores(waitInfo, kInfiniteWait));

		// GPU has signaled that resources are free to use so its time to fetch them and prep for recording commands.
		const FrameResources& frameResources = gFramesInFlight[frameResourceIndex];
		AssertVk(Graphics::gVkDevice.resetCommandPool(frameResources.commandPool));

		// This semaphore will be used to check if the image we are going to be rendering to later is free to write to.
		const vk::Semaphore imageAcquireSemaphore = frameResources.imageAcquiredSemaphore;

		// Ask the device if it has a swapchain index it knows is going to be available for writes later and bind it
		// with the semaphore.
		auto imageAcquireResultValue = Graphics::gVkDevice.acquireNextImageKHR(
		    Graphics::gSwapchain.swapchain, kInfiniteWait, imageAcquireSemaphore
		);

		// Image is out of date and swapchain must be recreated.
		if (imageAcquireResultValue.result == vk::Result::eErrorOutOfDateKHR)
		{
			sShouldRecreateSwapchain = true;
			return;
		}
		// Image is suboptimal and swapchain should be recreated but the frame can continue.
		else if (imageAcquireResultValue.result == vk::Result::eSuboptimalKHR)
		{
			sShouldRecreateSwapchain = true;
		}
		else
		{
			// If not any of the plausible values above, assert it to check if its an error.
			AssertVk(imageAcquireResultValue.result);
		}

		const uint32_t imageIndex             = imageAcquireResultValue.value;
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
			        .image            = *gSceneDepth.Get(),
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

			constexpr std::array clearColor                   = {0.01f, 0.01f, 0.01f, 1.0f};
			const vk::RenderingAttachmentInfo colorAttachInfo = {
			    .imageView   = swapchainView,
			    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			    .loadOp      = vk::AttachmentLoadOp::eClear,  // Clear image
			    .storeOp     = vk::AttachmentStoreOp::eStore, // Keep data for presentation
			    .clearValue  = {.color = {.float32 = clearColor}}
			};

			const vk::RenderingAttachmentInfo depthAttachInfo = {
			    .imageView   = gSceneDepth.Get()->view,
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
				    .x = 0, .y = 0, .width = static_cast<float>(targetWidth), .height = static_cast<float>(targetHeight)
				};
				commandBuffer.setViewport(0, 1, &viewport);

				const vk::Rect2D scissor = {
				    .offset = {.x = 0, .y = 0}, .extent = {.width = targetWidth, .height = targetHeight}
				};
				commandBuffer.setScissor(0, 1, &scissor);

				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, gHelloTrianglePipeline);
				commandBuffer.draw(3, 1, 0, 0);
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
			// image at that index to actually be available. This semaphore is tied to the swapchain image so as soon as
			// the image at the index that was given to prior can be written to, a signal is sent.
			// NOTE: This is not a CPU wait. The GPU will run all commands submitted but will stop and wait for the
			// semaphore to be signaled at the point of trying to bind the image for color output in the pipeline.
			// Earlier pipeline stages are free to run.
			// TODO: Work should be split into different command buffers because this wait waits on the FIRST instance
			// of color output attachement, even if the target is not the swapchain. This wait should only be for the
			// command buffer that has commands that will write to the swapchain, all other kind of work should be
			// separate to avoid unecessary GPU stalls.
			const vk::SemaphoreSubmitInfo imageAcquiredWaitInfo = {
			    .semaphore = imageAcquireSemaphore, .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
			};

			const std::array semaphoreSignals = {
			    // This binary semaphore is used by the presentation engine. The GPU will signal this semaphore once all
			    // GRAPHICS commmands has been completed, meaning the swapchain image is ready to be presented.
			    vk::SemaphoreSubmitInfo{
			        .semaphore = renderingCompleteSemaphore, .stageMask = vk::PipelineStageFlagBits2::eAllGraphics
			    },
			    // Once ALL commands are done (not only graphics), signal the timeline semaphore with this frames signal
			    // value to finally communicate that all resources associated with this frame has been completed.
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
			AssertVk(graphicsQueue.presentKHR(presentInfo));
		}
	}
} // namespace Renderer
