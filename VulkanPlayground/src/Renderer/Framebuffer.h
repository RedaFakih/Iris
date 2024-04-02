#pragma once

#include "Core/Base.h"
#include "Renderer/Core/Vulkan.h"
#include "Texture.h"

#include <glm/glm.hpp>

namespace vkPlayground {

	 /*
	  * IMPORTANT:
	  * In case of using Multi Sampled attachments:
	  *	 - You will not be able to use the framebuffer images directly instead you will have to use the resolve images...
	  *	 - In case of Samples = 1 (Not multi sampled images) the resolve images will not be created or allocated so you can use the framebuffer images directly.
	  * 
	  * The way this works now is:
	  *      You can specify in the specification if an attachment will be sampled or not later that affects the final layout of the attachment
	  *          - Will be sampled -> finalLayout: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL / VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
	  *          - Will NOT be sampled -> finalLayout: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL / VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	  *
	  * The way of usage will be as follows:
	  *   Say you have a render pass that will do multiple passes on the same image or references an existing image then the Sampled flag should NOT be set
	  *   BUT then you decide to sample from the image... It will be your responsibility to set a pipeline ImageMemoryBarrier to make sure the layout is transitioned to
	  *   the correct layout and also make sure there are no synchronization hazards {Ref: 1}
	  *
	  * References:
	  *  1: <https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkAccessFlagBits.html> (Scroll to find table of VK_ACCESS_X -> VK_PIPELINE_STAGE_X to solve any hazard issue)
	  *  2: <https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples-(Legacy-synchronization-APIs)>
	  *  3: <https://www.reddit.com/r/vulkan/comments/8arvcj/a_question_about_subpass_dependencies/>
	  *  4: <https://github.com/ARM-software/vulkan-sdk/blob/master/samples/multipass/multipass.cpp> (line: 829)
	  */

	enum class FramebufferBlendMode
	{
		None = 0,
		OneZero,
		SrcAlphaOneMinusSrcAlpha,
		Additive,
		ZeroSrcColor
	};

	// Weird name but this is to specify whether the attachment is an existing one or a new one basically...
	enum class AttachmentPassThroughUsage
	{
		None = 0,
		Input, // Create a new image or takes an existing one and sets the final layout to be setup to be an input attachment for upcoming steps...
		Sampled // Create a new one and set the final layout to be ready for sampling
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

		FramebufferTextureSpecification(ImageFormat format, AttachmentLoadOp loadOp) : Format(format), LoadOp(loadOp) {}

		FramebufferTextureSpecification(ImageFormat format, AttachmentPassThroughUsage sampled) : Format(format), Sampled(sampled) {}

		FramebufferTextureSpecification(ImageFormat format, bool blend, FramebufferBlendMode mode) 
			: Format(format), Blend(blend), BlendMode(mode) {}

		FramebufferTextureSpecification(ImageFormat format, bool blend, FramebufferBlendMode mode, AttachmentLoadOp loadop)
			: Format(format), Blend(blend), BlendMode(mode), LoadOp(loadop) {}

		FramebufferTextureSpecification(ImageFormat format, bool blend, FramebufferBlendMode mode, AttachmentLoadOp loadop, TextureFilter filter)
			: Format(format), Blend(blend), BlendMode(mode), LoadOp(loadop), FilterMode(filter) {}

		FramebufferTextureSpecification(ImageFormat format, bool blend, FramebufferBlendMode mode, AttachmentLoadOp loadop, TextureFilter filter, TextureWrap wrap)
			: Format(format), Blend(blend), BlendMode(mode), LoadOp(loadop), FilterMode(filter), WrapMode(wrap) {}

		ImageFormat Format;
		AttachmentPassThroughUsage Sampled = AttachmentPassThroughUsage::Sampled; // Put to Input in order to set final layout to be ATTACHMENT_OPTIMAL if you want to use as input attachment later
		bool Blend = true;
		FramebufferBlendMode BlendMode = FramebufferBlendMode::SrcAlphaOneMinusSrcAlpha;
		AttachmentLoadOp LoadOp = AttachmentLoadOp::Inherit; // Deduces load operation from framebuffer specification parameters
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

		// Framebuffer scale (Rendering scale of the framebuffer)
		float Scale = 1.0f;

		// Clear settings...
		bool ClearColorOnLoad = true;
		glm::vec4 ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		bool ClearDepthOnLoad = true;
		float DepthClearValue = 0.0f;

		FramebufferAttachmentSpecification Attachments;

		// Multisampling framebuffer... (1, 2, 4, 8, 16, 32, 64). DO NOT SET THE RESOLVE ATTACHMENT IF Samples == 1.
		uint32_t Samples = 1;

		// Global blending mode for framebuffer (individual attachments could be disabled in FramebufferTextureSpecification)
		bool Blend = true;
		// None means use the blending mode in FramebufferTextureSpecification
		FramebufferBlendMode BlendMode = FramebufferBlendMode::None;

		// Screenbuffer? Render to swapchain and dont create a framebuffer 
		bool SwapchainTarget = false;

		// Will the framebuffer be used for Transfer ops? Sets the Transfer for texture specification to `true`
		bool Transfer = false;

		std::map<uint32_t, Ref<Texture2D>> ExistingImages;
	};

	// This will hold the actual VkRenderPass Object however it will be in the PipelineSpecification
	class Framebuffer : public RefCountedObject
	{
	public:
		Framebuffer(const FramebufferSpecification& spec);
		~Framebuffer();

		[[nodiscard]] static Ref<Framebuffer> Create(const FramebufferSpecification& spec);

		void Invalidate();
		void Resize(uint32_t width, uint32_t height, bool forceRecreate = false);
		void Release();

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		std::size_t GetColorAttachmentCount() const { return m_Specification.SwapchainTarget ? 1 : m_ColorAttachmentImages.size(); }
		bool HasDepthAttachment() const { return m_DepthAttachmentImage != nullptr; }

		Ref<Texture2D> GetImage(uint32_t attachmentIndex = 0) const { VKPG_ASSERT(attachmentIndex < m_ColorAttachmentImages.size()); return m_ColorAttachmentImages[attachmentIndex]; }
		Ref<Texture2D> GetDepthImage() const { return m_DepthAttachmentImage; }

		Ref<Texture2D> GetResolveImage(uint32_t attachmentIndex = 0) const { VKPG_ASSERT(attachmentIndex < m_ColorResolveImages.size()); return m_ColorResolveImages[attachmentIndex]; }
		Ref<Texture2D> GetDepthResolveImage() const { return m_DepthResolveImage; }

		VkRenderPass GetVulkanRenderPass() const { return m_VulkanRenderPass; }
		VkFramebuffer GetVulkanFramebuffer() const { return m_VulkanFramebuffer; }
		const std::vector<VkClearValue>& GetVulkanClearValues() const { return m_ClearValues; }

		FramebufferSpecification& GetSpecification() { return m_Specification; }
		const FramebufferSpecification& GetSpecification() const { return m_Specification; }

	private:
		FramebufferSpecification m_Specification;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		std::vector<Ref<Texture2D>> m_ColorAttachmentImages;
		Ref<Texture2D> m_DepthAttachmentImage = nullptr;

		std::vector<Ref<Texture2D>> m_ColorResolveImages;
		Ref<Texture2D> m_DepthResolveImage = nullptr;
		
		std::vector<VkClearValue> m_ClearValues;

		VkRenderPass m_VulkanRenderPass = nullptr;
		VkFramebuffer m_VulkanFramebuffer = nullptr;

	};

}