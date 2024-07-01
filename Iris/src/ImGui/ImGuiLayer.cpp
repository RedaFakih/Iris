#include "IrisPCH.h"
#include "ImGuiLayer.h"

#include "Core/Application.h"
#include "FontAwesome.h"
#include "ImGuiFonts.h"
#include "ImGuizmo.h"
#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/Vulkan.h"
#include "Renderer/Renderer.h"
#include "Themes.h"

#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

namespace Iris {

	static VkDescriptorPool s_ImGuiDescriptorPool = nullptr;

    ImGuiLayer::ImGuiLayer()
    {
    }

    ImGuiLayer::~ImGuiLayer()
    {
    }

    ImGuiLayer* ImGuiLayer::Create()
    {
        return new ImGuiLayer();
    }

    void ImGuiLayer::OnAttach()
    {
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
		io.FontGlobalScale = 1.0f;
		// io.FontAllowUserScaling = true;
		io.BackendPlatformName = "Win32";
		io.BackendRendererName = "Iris Engine - Vulkan";

		// Configure Fonts
		LoadFonts();

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		SetDarkThemeColors();
		
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 6.0f;
			style.PopupRounding = 3.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, style.Colors[ImGuiCol_WindowBg].w);

		Renderer::Submit([]()
		{
			Application& app = Application::Get();
			GLFWwindow* window = app.GetWindow().GetNativeWindow();
			VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

			// This descriptor pool serves the purpose which is mainly allocating descriptor sets for the font textures of ImGui because other image descriptors
			// will be handled by the Renderer's descriptor pool
			// Create Descriptor Pool (vkGuide)
			constexpr VkDescriptorPoolSize poolSizes[] =
			{
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
			};

			VkDescriptorPoolCreateInfo descPoolCI = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.maxSets = 100 * IM_ARRAYSIZE(poolSizes),
				.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes)),
				.pPoolSizes = poolSizes
			};
			VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descPoolCI, nullptr, &s_ImGuiDescriptorPool));

			ImGui_ImplGlfw_InitForVulkan(window, true);
			ImGui_ImplVulkan_InitInfo initInfo = {
				.Instance = RendererContext::GetInstance(),
				.PhysicalDevice = RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetVulkanPhysicalDevice(),
				.Device = device,
				.QueueFamily = static_cast<uint32_t>(RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics),
				.Queue = RendererContext::GetCurrentDevice()->GetGraphicsQueue(),
				.PipelineCache = VK_NULL_HANDLE,
				.DescriptorPool = s_ImGuiDescriptorPool,
				.MinImageCount = Renderer::GetConfig().FramesInFlight,
				.ImageCount = app.GetWindow().GetSwapChain().GetImageCount(),
				.Allocator = nullptr,
				.ColorAttachmentFormat = app.GetWindow().GetSwapChain().GetColorFormat(),
				.CheckVkResultFn = Utils::VulkanCheckResult
			};
			ImGui_ImplVulkan_Init(&initInfo);

			// Upload fonts
			{
				VkCommandBuffer commandBuffer = RendererContext::GetCurrentDevice()->GetCommandBuffer(true);
				ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
				RendererContext::GetCurrentDevice()->FlushCommandBuffer(commandBuffer);

				ImGui_ImplVulkan_DestroyFontUploadObjects();
			}
		});
    }

    void ImGuiLayer::OnDetach()
    {
        VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

		Renderer::SubmitReseourceFree([device, descriptorPool = s_ImGuiDescriptorPool]()
		{
			vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		});

		Renderer::Submit([device]()
		{
			VK_CHECK_RESULT(vkDeviceWaitIdle(device));
			ImGui_ImplVulkan_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		});
    }

	void ImGuiLayer::Begin()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiLayer::End()
	{
		ImGui::Render();

		SwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();

		uint32_t width = swapChain.GetWidth();
		uint32_t height = swapChain.GetHeight();
		uint32_t commandBufferIndex = swapChain.GetCurrentBufferIndex();

		VkCommandBufferBeginInfo drawCmdBufferBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};

		VkCommandBuffer drawCommandBuffer = swapChain.GetCurrentDrawCommandBuffer();
		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCommandBuffer, &drawCmdBufferBeginInfo));

		VkDebugUtilsLabelEXT debugLabel = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pLabelName = "ImGui - RenderPass"
		};
		glm::vec4 debugLabelMarkerColor = { 1.0f, 0.0f, 0.317f, 1.0f };
		std::memcpy(&debugLabel.color, glm::value_ptr(debugLabelMarkerColor), 4 * sizeof(float));
		fpCmdBeginDebugUtilsLabelEXT(drawCommandBuffer, &debugLabel);

		swapChain.TransitionImageLayout(SwapChain::ImageLayout::Attachment);

		VkRenderingAttachmentInfo imageAttachmentInfo = swapChain.GetCurrentImageInfo();
		VkRenderingInfo renderingInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.renderArea = {
				.offset = { .x = 0, .y = 0 },
				.extent = { .width = width, .height = height }
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &imageAttachmentInfo,
			.pDepthAttachment = nullptr,
			.pStencilAttachment = nullptr
		};

		vkCmdBeginRendering(drawCommandBuffer, &renderingInfo);

		VkViewport viewport = {
			.x = 0.0f,
			.y = static_cast<float>(height),
			.width = static_cast<float>(width),
			.height = -static_cast<float>(height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};
		vkCmdSetViewport(drawCommandBuffer, 0, 1, &viewport);

		VkRect2D scissor = {
			.offset = { .x = 0, .y = 0 },
			.extent = { .width = width, .height = height }
		};
		vkCmdSetScissor(drawCommandBuffer, 0, 1, &scissor);

		ImDrawData* mainDrawData = ImGui::GetDrawData();
		ImGui_ImplVulkan_RenderDrawData(mainDrawData, drawCommandBuffer);

		vkCmdEndRendering(drawCommandBuffer);

		swapChain.TransitionImageLayout(SwapChain::ImageLayout::Present);

		fpCmdEndDebugUtilsLabelEXT(drawCommandBuffer);

		VK_CHECK_RESULT(vkEndCommandBuffer(drawCommandBuffer));

		// Update and Render additional Platform Windows
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

    void ImGuiLayer::SetDarkThemeColors()
    {
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4 (&colors)[55] = ImGui::GetStyle().Colors;

		//========================================================
		/// Colors

		// Headers
		colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::GroupHeader);
		colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::GroupHeader);
		colors[ImGuiCol_HeaderActive] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::GroupHeader);

		// Buttons
		colors[ImGuiCol_Button] = ImColor(56, 56, 56, 200);
		colors[ImGuiCol_ButtonHovered] = ImColor(70, 70, 70, 255);
		colors[ImGuiCol_ButtonActive] = ImColor(56, 56, 56, 150);

		// Frame BG
		colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::PropertyField);
		colors[ImGuiCol_FrameBgHovered] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::PropertyField);
		colors[ImGuiCol_FrameBgActive] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::PropertyField);

		// Tabs
		colors[ImGuiCol_Tab] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::Titlebar);
		colors[ImGuiCol_TabHovered] = ImColor(135, 225, 255, 30);
		colors[ImGuiCol_TabActive] = ImColor(135, 225, 255, 60);
		colors[ImGuiCol_TabUnfocused] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::Titlebar);
		colors[ImGuiCol_TabUnfocusedActive] = colors[ImGuiCol_TabHovered];

		// Title
		colors[ImGuiCol_TitleBg] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::Titlebar);
		colors[ImGuiCol_TitleBgActive] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::Titlebar);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Resize Grip
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);

		// Scrollbar
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.0f);

		// Check Mark
		colors[ImGuiCol_CheckMark] = ImColor(200, 200, 200, 255);

		// Slider
		colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 0.7f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.66f, 0.66f, 0.66f, 1.0f);

		// Text
		colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::Text);

		// Checkbox
		colors[ImGuiCol_CheckMark] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::Text);

		// Separator
		colors[ImGuiCol_Separator] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::BackgroundDark);
		colors[ImGuiCol_SeparatorActive] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::Highlight);
		colors[ImGuiCol_SeparatorHovered] = ImColor(39, 185, 242, 150);

		// Window Background
		colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::Titlebar);
		colors[ImGuiCol_ChildBg] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::Background);
		colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::BackgroundPopup);
		colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::BackgroundDark);

		// Tables
		colors[ImGuiCol_TableHeaderBg] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::GroupHeader);
		colors[ImGuiCol_TableBorderLight] = ImGui::ColorConvertU32ToFloat4(Colors::Theme::BackgroundDark);

		// Menubar
		colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f };

		//========================================================
		/// Style
		style.FrameRounding = 2.5f;
		style.FrameBorderSize = 1.0f;
		style.IndentSpacing = 11.0f;
    }

	void ImGuiLayer::LoadFonts()
	{
		FontSpecification robotoLarge = {
			.FontName = "RobotoLarge",
			.Filepath = "Resources/Editor/Fonts/Roboto/Roboto-Regular.ttf",
			.Size = 24.0f
		};
		m_FontsLibrary.Load(robotoLarge);

		FontSpecification robotoBold = {
			.FontName = "RobotoBold",
			.Filepath = "Resources/Editor/Fonts/Roboto/Roboto-Bold.ttf",
			.Size = 18.0f
		};
		m_FontsLibrary.Load(robotoBold);

		FontSpecification robotoDefault = {
			.FontName = "RobotoDefault",
			.Filepath = "Resources/Editor/Fonts/Roboto/Roboto-SemiMedium.ttf",
			.Size = 15.0f
		};
		m_FontsLibrary.Load(robotoDefault, true);

		static constexpr ImWchar s_FontAwesomeGlyphRanges[] = { IR_ICON_MIN_FA, IR_ICON_MAX_FA, 0 };
		FontSpecification fontAwesome = {
			.FontName = "FontAwesome",
			.Filepath = "Resources/Editor/Fonts/FontAwesome/fontawesome-webfont.ttf",
			.Size = 16.0f,
			.GlyphRanges = s_FontAwesomeGlyphRanges,
			.MergeWithLast = true
		};
		m_FontsLibrary.Load(fontAwesome);
	}

}