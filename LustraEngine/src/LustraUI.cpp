#include "LustraUI.h"

#include "Graphics.h"
#include "LustraLib/Assert.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include <cstddef>

namespace
{
	constexpr uint32_t kImGuiTextureDescriptorCount = 512u;
} // namespace

namespace Lustra::UI
{
	void Initialize(void* window)
	{
		// Setup ImGui context
		{
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			auto& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

			ImGui::StyleColorsDark();

			// Setup scaling
			const float mainScaling = 1.0f;

			ImGuiStyle& style = ImGui::GetStyle();
			style.ScaleAllSizes(mainScaling);
			style.FontScaleDpi = mainScaling;
		}

		ImGui_ImplVulkan_InitInfo imguiInitInfo = {};

		imguiInitInfo.ApiVersion          = Graphics::gTargetVulkanVersion;
		imguiInitInfo.Instance            = Graphics::gVkInstance;
		imguiInitInfo.PhysicalDevice      = Graphics::gVkPhysicalDevice;
		imguiInitInfo.Device              = Graphics::gVkDevice;
		imguiInitInfo.QueueFamily         = Graphics::graphicsQueue.index;
		imguiInitInfo.Queue               = Graphics::graphicsQueue.queue;
		imguiInitInfo.ImageCount          = static_cast<uint32_t>(Graphics::gSwapchain.images.size());
		imguiInitInfo.MinImageCount       = imguiInitInfo.ImageCount;
		imguiInitInfo.UseDynamicRendering = true;
		imguiInitInfo.Allocator           = *Graphics::gAllocationCallbacks;
		imguiInitInfo.DescriptorPoolSize =
		    std::max(IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE, static_cast<int>(kImGuiTextureDescriptorCount));

		vk::PipelineRenderingCreateInfo renderCreateInfo = {
		    .viewMask              = 0, // Single view rendering.
		    .depthAttachmentFormat = vk::Format::eUndefined,
		};
		renderCreateInfo.setColorAttachmentFormats(Graphics::gTargetSurfaceFormat.format);

		imguiInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo = renderCreateInfo;

		ImGui_ImplSDL3_InitForVulkan(reinterpret_cast<SDL_Window*>(window));
		ImGui_ImplVulkan_Init(&imguiInitInfo);
	}

	void ProcessEvent(SDL_Event* event)
	{
		ENSURE(event != nullptr);

		ImGui_ImplSDL3_ProcessEvent(event);
	}

	void NewFrame()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
	}

	void EndFrame()
	{
		ImGui::EndFrame();
	}

	void RenderAndEndFrame(vk::CommandBuffer commandBuffer, vk::Image image, vk::ImageView view)
	{
		const vk::ImageMemoryBarrier2 sceneToUi = {
		    .srcStageMask     = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		    .srcAccessMask    = vk::AccessFlagBits2::eColorAttachmentWrite,
		    .dstStageMask     = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		    .dstAccessMask    = vk::AccessFlagBits2::eColorAttachmentWrite |
		                        vk::AccessFlagBits2::eColorAttachmentRead, // Load + blend both read
		    .oldLayout        = vk::ImageLayout::eColorAttachmentOptimal,
		    .newLayout        = vk::ImageLayout::eColorAttachmentOptimal, // same role, no transition
		    .image            = image,
		    .subresourceRange = {
		        .aspectMask     = vk::ImageAspectFlagBits::eColor,
		        .baseMipLevel   = 0,
		        .levelCount     = 1,
		        .baseArrayLayer = 0,
		        .layerCount     = 1
		    },
		};

		const vk::DependencyInfo dep = {.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &sceneToUi};
		commandBuffer.pipelineBarrier2(dep);

		const vk::RenderingAttachmentInfo renderTargetAttachement = {
		    .imageView   = view,
		    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		    .loadOp      = vk::AttachmentLoadOp::eLoad,
		    .storeOp     = vk::AttachmentStoreOp::eStore
		};

		vk::RenderingInfo renderInfo = {
		    .renderArea =
		        vk::Rect2D{
		            .extent =
		                {
		                    .width  = Graphics::gSwapchain.width,
		                    .height = Graphics::gSwapchain.height,
		                },
		        },
		    .layerCount           = 1,
		    .colorAttachmentCount = 1,
		    .pColorAttachments    = &renderTargetAttachement
		};
		commandBuffer.beginRendering(renderInfo);

		static bool showDemo = true;
		ImGui::ShowDemoWindow(&showDemo);

		// Draw all ui.
		{
			ImGui::Render();
			ImDrawData* drawData   = ImGui::GetDrawData();
			const bool isMinimized = (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f);
			if (!isMinimized)
			{
				ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
			}
		}

		commandBuffer.endRendering();
	}

	void Destroy()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}
} // namespace Lustra::UI
