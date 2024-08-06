#include "IrisPCH.h"
#include "Framebuffer.h"

#include "Renderer.h"

namespace Iris {

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

    namespace Utils {

        inline VkAttachmentLoadOp GetVulkanAttachmentLoadOp(const FramebufferSpecification& spec, const FramebufferTextureSpecification& attachmentSpec)
        {
            if (attachmentSpec.LoadOp == AttachmentLoadOp::Inherit)
            {
                if (Utils::IsDepthFormat(attachmentSpec.Format))
                {
                    return spec.ClearDepthOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
                }

                return spec.ClearColorOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            }

            return attachmentSpec.LoadOp == AttachmentLoadOp::Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        }

    }

    Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
    {
        return CreateRef<Framebuffer>(spec);
    }

    Framebuffer::Framebuffer(const FramebufferSpecification& specification)
        : m_Specification(specification)
    {
        if (m_Specification.Width == 0)
        {
            m_Width = Application::Get().GetWindow().GetWidth();
            m_Height = Application::Get().GetWindow().GetHeight();
        }
        else
        {
            m_Width = specification.Width;
            m_Height = specification.Height;
        }

        // Create image objects...
        uint32_t attachmentIndex = 0;
        for (auto& attachmentSpec : m_Specification.Attachments.Attachments)
        {
            if (Utils::IsDepthFormat(attachmentSpec.Format))
            {
                TextureSpecification spec = {
                    .DebugName = fmt::format("{} - DepthAttachment{}", m_Specification.DebugName.empty() ? "Unnamed FB" : m_Specification.DebugName, attachmentIndex),
                    .Width = m_Width,
                    .Height = m_Height,
                    .Format = attachmentSpec.Format,
                    .Usage = ImageUsage::Attachment,
                    .Samples = 1,
                    .Transfer = m_Specification.Transfer
                };
                m_DepthAttachmentImage = Texture2D::CreateNull(spec);
            }
            else
            {
                TextureSpecification spec = {
                    .DebugName = fmt::format("{} - ColorAttachment{}", m_Specification.DebugName.empty() ? "Unnamed FB" : m_Specification.DebugName, attachmentIndex),
                    .Width = m_Width,
                    .Height = m_Height,
                    .Format = attachmentSpec.Format,
                    .Usage = ImageUsage::Attachment,
                    .Samples = 1,
                    .Transfer = m_Specification.Transfer
                };
                m_ColorAttachmentImages.emplace_back(Texture2D::CreateNull(spec));
            }

            ++attachmentIndex;
        }

        IR_ASSERT(specification.Attachments.Attachments.size());
        Resize(m_Width, m_Height, true);
    }

    Framebuffer::~Framebuffer()
    {
        Release();
    }

    void Framebuffer::Invalidate()
    {
        Ref<Framebuffer> instance = this;
        Renderer::Submit([instance]() mutable
        {
            instance->RT_Invalidate();
        });
    }

    void Framebuffer::RT_Invalidate()
    {
        Release();

        uint32_t attachmentIndex = 0;
        for (const auto& attachmentSpec : m_Specification.Attachments.Attachments)
        {
            if (Utils::IsDepthFormat(attachmentSpec.Format))
            {
                if (m_Specification.ExistingImages.contains(attachmentIndex))
                {
                    Ref<Texture2D> existingImage = m_Specification.ExistingImages[attachmentIndex];
                    IR_VERIFY(Utils::IsDepthFormat(existingImage->GetFormat()), "Trying to attach a non-depth image to a depth attachment");
                    m_DepthAttachmentImage = existingImage;
                }
                else
                {
                    TextureSpecification& spec = m_DepthAttachmentImage->GetTextureSpecification();
                    spec.Width = m_Width;
                    spec.Height = m_Height;
                    m_DepthAttachmentImage->Invalidate();
                }

                bool hasLayerImageView = m_Specification.ExistingImageLayerViews.contains(attachmentIndex);
                m_DepthAttachmentInfo = VkRenderingAttachmentInfo{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext = nullptr,
                    .imageView = hasLayerImageView ? m_Specification.ExistingImageLayerViews.at(attachmentIndex)->GetVulkanImageView() : m_DepthAttachmentImage->GetVulkanImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // This is the layout that the image will be in DURING rendering. We still have to manually transition INTO and OUT OF
                    .loadOp = Utils::GetVulkanAttachmentLoadOp(m_Specification, attachmentSpec),
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE
                };

                if (m_Specification.ClearDepthOnLoad)
                {
                    m_DepthAttachmentInfo.clearValue = VkClearValue{
                        .depthStencil = { m_Specification.DepthClearValue, 0 }
                    };
                }

                m_DepthAttachmentImageFormat = Utils::GetVulkanImageFormat(attachmentSpec.Format);
            }
            else
            {
                if (m_Specification.ExistingImages.contains(attachmentIndex))
                {
                    Ref<Texture2D> existingImage = m_Specification.ExistingImages[attachmentIndex];
                    IR_ASSERT(!Utils::IsDepthFormat(existingImage->GetFormat()), "Trying to attach a non-color image to a color attachment");
                    m_ColorAttachmentImages[attachmentIndex] = existingImage;
                }
                else
                {
                    Ref<Texture2D> image = m_ColorAttachmentImages[attachmentIndex];
                    TextureSpecification& spec = image->GetTextureSpecification();
                    spec.Width = m_Width;
                    spec.Height = m_Height;
                    image->Invalidate();
                }

                bool hasLayerImageView = m_Specification.ExistingImageLayerViews.contains(attachmentIndex);
                VkRenderingAttachmentInfo& attachmentInfo = m_ColorAttachmentInfo.emplace_back();
                attachmentInfo = VkRenderingAttachmentInfo{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext = nullptr,
                    .imageView = hasLayerImageView ? m_Specification.ExistingImageLayerViews.at(attachmentIndex)->GetVulkanImageView() : m_ColorAttachmentImages[attachmentIndex]->GetVulkanImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = Utils::GetVulkanAttachmentLoadOp(m_Specification, attachmentSpec),
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE
                };

                if (m_Specification.ClearColorOnLoad)
                {
                    attachmentInfo.clearValue = VkClearValue{
                        .color = {
                            m_Specification.ClearColor.r,
                            m_Specification.ClearColor.g,
                            m_Specification.ClearColor.b,
                            m_Specification.ClearColor.a
                        }
                    };
                }

                m_ColorAttachmentImageFormats.emplace_back() = Utils::GetVulkanImageFormat(attachmentSpec.Format);
            }

            attachmentIndex++;
        }
    }

    void Framebuffer::Resize(uint32_t width, uint32_t height, bool forceRecreate)
    {
        if (!forceRecreate && (m_Width == width && m_Height == height))
            return;

        Ref<Framebuffer> instance = this;
        Renderer::Submit([instance, width, height]() mutable
        {
            instance->m_Width = width;
            instance->m_Height = height;

            if (!(instance->m_Specification.SwapchainTarget))
                instance->RT_Invalidate();
            else
            {
                instance->m_ColorAttachmentImageFormats.clear();

                // We get the first attachment since in case of SwapChainTarget we will only have one image in the specification which is the format of the swapchain image
                // TODO: Needs checking, Is this the right thing to do? The runtime works in debug mode but not in release mode tho...
                instance->m_ColorAttachmentImageFormats.emplace_back() = Utils::GetVulkanImageFormat(instance->m_Specification.Attachments.Attachments.front().Format);
            }
        });
    }

    void Framebuffer::Release()
    {
        // Here we just need to release the images that the framebuffer stores
        uint32_t attachmentIndex = 0;
        for (Ref<Texture2D> image : m_ColorAttachmentImages)
        {
            // Do not free the image if we do not own it...
            if (m_Specification.ExistingImages.contains(attachmentIndex))
                continue;
            image->Release();
            attachmentIndex++;
        }

        if (m_DepthAttachmentImage)
        {
            // Do we own it?
            uint32_t depthAttachmentIndex;
            uint32_t i = 0;
            for (const FramebufferTextureSpecification& attachmentSpec : m_Specification.Attachments.Attachments)
            {
                if (m_DepthAttachmentImage->GetFormat() == attachmentSpec.Format)
                {
                    depthAttachmentIndex = i;
                    break;
                }

                i++;
            }

            if (!m_Specification.ExistingImages.contains(depthAttachmentIndex))
                m_DepthAttachmentImage->Release();
        }

        // Clear attachment infos that clears all references to all image views
        m_ColorAttachmentInfo.clear();
        m_DepthAttachmentInfo = {};
    }

}