#pragma once

/*
 * TODO: Continue this abstraction after reaching the texturing section and all that stuff in the Vulkan-Tutorial
 */

#include "Core/Base.h"
#include "Renderer/Core/Vulkan.h"
#include "Image.h"

#include <glm/glm.hpp>

namespace vkPlayground {

	struct FramebufferSpecification
	{
		std::string DebugName;
		uint32_t Width = 1280;
		uint32_t Height = 720;
		glm::vec4 ClearColor = { 1.0f, 0.0f, 1.0f, 1.0f };

		// TODO: uint32_t Samples = 1;

		// Controls whether the frambuffer is resizable or not...
		bool Resizable = true;

		// Screenbuffer? Render to swapchain and dont create a framebuffer 
		bool SwapchainTarget = false;
	};

	// NOTE: This will hold the actual VkRenderPass Object however it will be in the PipelineSpecification and will have a custom images
	// and maybe in the future `existing images` in its specification... kind of like in Aurora with openGL
	class Framebuffer // : public RefCountedObject
	{
	public:
		Framebuffer() = default;
		Framebuffer(const FramebufferSpecification& spec);
		~Framebuffer();

		[[nodiscard]] static Ref<Framebuffer> Create();
		[[nodiscard]] static Ref<Framebuffer> Create(const FramebufferSpecification& spec);

		void Invalidate();
		void Resize(uint32_t width, uint32_t height, bool forceRecreate = false);
		void Release();

		VkRenderPass GetVulkanRenderPass() const { return m_VulkanRenderPass; }

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

	private:

		VkRenderPass m_VulkanRenderPass;
		VkFramebuffer m_Framebuffer;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
	};

}