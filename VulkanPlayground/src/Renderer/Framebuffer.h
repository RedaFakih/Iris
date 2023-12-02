#pragma once

#include "Core/Base.h"
#include "Renderer/Core/Vulkan.h"
#include "Texture.h"

#include <glm/glm.hpp>

namespace vkPlayground {

	enum class FramebufferBlendMode
	{
		None = 0,
		OneZero,
		SrcAlphaOneMinusSrcAlpha,
		Additive,
		ZeroSrcColor
	};

	enum class AttachmentLoadOp
	{
		None = 0,
		Inherit, // Sets the load op depending on specification parameters
		Clear,
		Load
	};

	struct FramebufferTextureSpecification
	{
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(ImageFormat format) : Format(format) {}
		FramebufferTextureSpecification(ImageFormat format, bool blend) : Format(format), Blend(blend) {}
		FramebufferTextureSpecification(ImageFormat format, bool blend, FramebufferBlendMode mode) 
			: Format(format), Blend(blend), BlendMode(mode) {}
		FramebufferTextureSpecification(ImageFormat format, bool blend, FramebufferBlendMode mode, AttachmentLoadOp loadop)
			: Format(format), Blend(blend), BlendMode(mode), LoadOp(loadop) {}
		FramebufferTextureSpecification(ImageFormat format, bool blend, FramebufferBlendMode mode, AttachmentLoadOp loadop, TextureFilter filter)
			: Format(format), Blend(blend), BlendMode(mode), LoadOp(loadop), FilterMode(filter) {}
		FramebufferTextureSpecification(ImageFormat format, bool blend, FramebufferBlendMode mode, AttachmentLoadOp loadop, TextureFilter filter, TextureWrap wrap)
			: Format(format), Blend(blend), BlendMode(mode), LoadOp(loadop), FilterMode(filter), WrapMode(wrap) {}

		ImageFormat Format;
		bool Blend = true;
		FramebufferBlendMode BlendMode = FramebufferBlendMode::SrcAlphaOneMinusSrcAlpha;
		AttachmentLoadOp LoadOp = AttachmentLoadOp::Inherit;
		TextureFilter FilterMode = TextureFilter::Linear;
		TextureWrap WrapMode = TextureWrap::Repeat;
	};

	struct FramebufferAttachmentSpecification
	{
		FramebufferAttachmentSpecification() = default;
		FramebufferAttachmentSpecification(const std::initializer_list<FramebufferTextureSpecification>& attachments)
			: Attachments(attachments) {}

		std::vector<FramebufferTextureSpecification> Attachments;
	};

	struct FramebufferSpecification
	{
		std::string DebugName;

		// Set to 0 to get the width and height of the application window
		uint32_t Width = 0;
		uint32_t Height = 0;

		// Framebuffer scale
		// TODO: float Scale = 1.0f; // rendering scale of the framebuffer

		// Clear settings...
		glm::vec4 ClearColor = { 1.0f, 0.0f, 1.0f, 1.0f };
		bool ClearColorOnLoad = true;
		float DepthClearValue = 0.0f;
		bool ClearDepthOnLoad = true;

		FramebufferAttachmentSpecification Attachments;

		// Multisampling framebuffer...
		uint32_t Samples = 1;

		// Global blending mode for framebuffer (individual attachments could be disabled in FramebufferTextureSpecification)
		bool Blend = true;
		// None means use the blending mode in FramebufferTextureSpecification
		FramebufferBlendMode BlendMode = FramebufferBlendMode::None;

		// Controls whether the frambuffer is resizable or not...
		bool Resizable = true;

		// Screenbuffer? Render to swapchain and dont create a framebuffer 
		bool SwapchainTarget = false;

		// Will the framebuffer be used for Transfer ops? Sets the Transfer for texture specification to `true`
		bool Transfer = false;
		// TODO: Existing images...
	};

	// This will hold the actual VkRenderPass Object however it will be in the PipelineSpecification and will have a custom images
	// and maybe in the future `existing images` in its specification... kind of like in Aurora with openGL
	class Framebuffer : public RefCountedObject
	{
	public:
		Framebuffer(const FramebufferSpecification& spec);
		~Framebuffer();

		[[nodiscard]] static Ref<Framebuffer> Create(const FramebufferSpecification& spec);

		void Invalidate();
		void Resize(uint32_t width, uint32_t height, bool forceRecreate = false);
		void Release();

		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }

		Ref<Texture2D> GetImage(uint32_t attachmentIndex = 0) const { PG_ASSERT(attachmentIndex < m_ColorAttachmentImages.size(), ""); return m_ColorAttachmentImages[attachmentIndex]; }
		Ref<Texture2D> GetDepthImage() const { return m_DepthAttachmentImage; }
		std::size_t GetColorAttachmentCount() const { return m_Specification.SwapchainTarget ? 1 : m_ColorAttachmentImages.size(); }
		bool HasDepthAttachment() const { return m_DepthAttachmentImage == nullptr; }

		VkRenderPass GetVulkanRenderPass() const { return m_VulkanRenderPass; }
		VkFramebuffer GetVulkanFramebuffer() const { return m_VulkanFramebuffer; }
		const std::vector<VkClearValue>& GetClearValues() const { return m_ClearValues; }

		FramebufferSpecification& GetSpecification() { return m_Specification; }
		const FramebufferSpecification& GetSpecification() const { return m_Specification; }

	private:
		FramebufferSpecification m_Specification;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		std::vector<Ref<Texture2D>> m_ColorAttachmentImages;
		Ref<Texture2D> m_DepthAttachmentImage;
		
		std::vector<VkClearValue> m_ClearValues;

		VkRenderPass m_VulkanRenderPass = nullptr;
		VkFramebuffer m_VulkanFramebuffer = nullptr;

	};

}