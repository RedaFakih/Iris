#pragma once

#include "Core/Base.h"
#include "Renderer/Core/Vulkan.h"
#include "Texture.h"

#include <glm/glm.hpp>

namespace Iris {

	 /*
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

		// NOTE: This is not generally needed since the scale of framebuffers is just set globally for all fraembuffers by the SceneRenderer
		// Framebuffer scale (Rendering scale of the framebuffer)
		// float Scale = 1.0f;

		// Clear settings...
		bool ClearColorOnLoad = true;
		glm::vec4 ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		bool ClearDepthOnLoad = true;
		float DepthClearValue = 0.0f;

		FramebufferAttachmentSpecification Attachments;

		// Global blending mode for framebuffer (individual attachments could be disabled in FramebufferTextureSpecification)
		bool Blend = true;
		// None means use the blending mode in FramebufferTextureSpecification
		FramebufferBlendMode BlendMode = FramebufferBlendMode::None;

		// Screenbuffer? Render to swapchain images.
		// NOTE: If this is set to true, Attachments should ONLY have one color attachment with RGBA format and NOTHING ELSE
		bool SwapchainTarget = false;

		// Will the framebuffer be used for Transfer ops? Sets the Transfer for texture specification to `true`
		bool Transfer = false;

		std::map<uint32_t, Ref<Texture2D>> ExistingImages;
	};

	class Framebuffer : public RefCountedObject
	{
	public:
		Framebuffer(const FramebufferSpecification& specification);
		~Framebuffer();

		[[nodiscard]] static Ref<Framebuffer> Create(const FramebufferSpecification& spec);

		void Invalidate();
		void RT_Invalidate();
		void Resize(uint32_t width, uint32_t height, bool forceRecreate = false);
		void Release();

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		std::size_t GetColorAttachmentCount() const { return m_Specification.SwapchainTarget ? 1 : m_ColorAttachmentImages.size(); }
		bool HasDepthAttachment() const { return m_DepthAttachmentImage != nullptr; }

		Ref<Texture2D> GetImage(uint32_t attachmentIndex = 0) const { IR_ASSERT(attachmentIndex < m_ColorAttachmentImages.size()); return m_ColorAttachmentImages[attachmentIndex]; }
		Ref<Texture2D> GetDepthImage() const { return m_DepthAttachmentImage; }

		const std::vector<VkRenderingAttachmentInfo>& GetColorAttachmentInfos() const { return m_ColorAttachmentInfo; }
		const VkRenderingAttachmentInfo& GetDepthAttachmentInfo() const { return m_DepthAttachmentInfo; }

		const std::vector<VkFormat>& GetColorAttachmentImageFormats() const { return m_ColorAttachmentImageFormats; }
		const VkFormat& GetDepthAttachmentImageFormat() const { return m_DepthAttachmentImageFormat; }

		FramebufferSpecification& GetSpecification() { return m_Specification; }
		const FramebufferSpecification& GetSpecification() const { return m_Specification; }

	private:
		FramebufferSpecification m_Specification;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		std::vector<Ref<Texture2D>> m_ColorAttachmentImages;
		Ref<Texture2D> m_DepthAttachmentImage = nullptr;

		std::vector<VkRenderingAttachmentInfo> m_ColorAttachmentInfo;
		VkRenderingAttachmentInfo m_DepthAttachmentInfo = {};

		std::vector<VkFormat> m_ColorAttachmentImageFormats;
		VkFormat m_DepthAttachmentImageFormat = VK_FORMAT_MAX_ENUM;

	};

}