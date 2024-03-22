#include "vkPch.h"
#include "Framebuffer.h"

#include "Renderer.h"

namespace vkPlayground {

    /*
     * The way this works now is:
     *      You can specify in the specification if an attachment will be sampled or not later that affects the final layout of the attachment
     *          - Will be sampled -> finalLayout: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL / VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
     *          - Will NOT be sampled -> finalLayout: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL / VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
     * 
     * The way of usage will be as follows:
     *   Say you have a render pass that will do multiple passes on the same image or references an existing image then the Sampled flag should NOT be set
     *   BUT then you decide to sample from the image... It will be your responsibility to set a pipeline ImageMemoryBarrier to make sure the layout is transitioned to
     *   the correct layout and also make sure there are no synchronization hazards {Ref: 4}
     * 
     * 
     * 
     * 
     * References:
     *  1: <https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples-(Legacy-synchronization-APIs)>
     *  2: <https://www.reddit.com/r/vulkan/comments/8arvcj/a_question_about_subpass_dependencies/>
     *  3: <https://github.com/ARM-software/vulkan-sdk/blob/master/samples/multipass/multipass.cpp> (line: 829)
     *  4: <https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkAccessFlagBits.html> (Scroll to find table of VK_ACCESS_X -> VK_PIPELINE_STAGE_X)
     */

    namespace Utils {

        inline VkAttachmentLoadOp GetVulkanAttachmentLoadOp(const FramebufferSpecification& spec, const FramebufferTextureSpecification& attachmentSpec)
        {
            if (attachmentSpec.LoadOp == AttachmentLoadOp::Inherit)
            {
                if (Utils::IsDepthFormat(attachmentSpec.Format))
                    return spec.ClearDepthOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;

                return spec.ClearColorOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            }

            return attachmentSpec.LoadOp == AttachmentLoadOp::Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        }

        inline VkSampleCountFlagBits GetSamplerCount(uint32_t samples)
        {
            switch (samples)
            {
                case 1:     return VK_SAMPLE_COUNT_1_BIT;
                case 2:     return VK_SAMPLE_COUNT_2_BIT;
                case 4:     return VK_SAMPLE_COUNT_4_BIT;
                case 8:     return VK_SAMPLE_COUNT_8_BIT;
                case 16:    return VK_SAMPLE_COUNT_16_BIT;
                case 32:    return VK_SAMPLE_COUNT_32_BIT;
                case 64:    return VK_SAMPLE_COUNT_64_BIT;
            }

            return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
        }
    }

    Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
    {
        return CreateRef<Framebuffer>(spec);
    }

    Framebuffer::Framebuffer(const FramebufferSpecification& spec)
        : m_Specification(spec)
    {
        if (m_Specification.Width == 0)
        {
            m_Width = Application::Get().GetWindow().GetWidth();
            m_Height = Application::Get().GetWindow().GetHeight();
        }
        else
        {
            m_Width = spec.Width;
            m_Height = spec.Height;
        }

        // Create image objects...
        uint32_t attachmentIndex = 0;
        for (auto& attachmentSpec : m_Specification.Attachments.Attachments)
        {
            if (Utils::IsDepthFormat(attachmentSpec.Format))
            {
                TextureSpecification spec = {
                    .DebugName = fmt::format("{} - DepthAttachment{}", m_Specification.DebugName.empty() ? "Unamed FB" : m_Specification.DebugName, attachmentIndex),
                    .Width = m_Width,
                    .Height = m_Height,
                    .Format = attachmentSpec.Format,
                    .Usage = ImageUsage::Attachment,
                    .Trasnfer = m_Specification.Transfer,
                };
                m_DepthAttachmentImage = Texture2D::CreateNull(spec);
            }
            else
            {
                TextureSpecification spec = {
                    .DebugName = fmt::format("{} - ColorAttachment{}", m_Specification.DebugName.empty() ? "Unamed FB" : m_Specification.DebugName, attachmentIndex),
                    .Width = m_Width,
                    .Height = m_Height,
                    .Format = attachmentSpec.Format,
                    .Usage = ImageUsage::Attachment,
                    .Trasnfer = m_Specification.Transfer,
                };
                m_ColorAttachmentImages.emplace_back(Texture2D::CreateNull(spec));
            }

            ++attachmentIndex;
        }

        PG_ASSERT(spec.Attachments.Attachments.size(), "");
        Resize(m_Width, m_Height, true);
    }

    Framebuffer::~Framebuffer()
    {
        Release();
    }

    void Framebuffer::Invalidate()
    {
        VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

        Release();

        m_ClearValues.reserve(m_Specification.Attachments.Attachments.size());

        std::vector<VkAttachmentDescription> attachmentDescriptions;

        std::vector<VkAttachmentReference> colorAttachmentReferences;
        VkAttachmentReference depthAttachmentReference;

        uint32_t attachmentIndex = 0;
        for (const auto& attachmentSpec : m_Specification.Attachments.Attachments)
        {
            if (Utils::IsDepthFormat(attachmentSpec.Format))
            {
                if (m_Specification.ExistingImages.contains(attachmentIndex))
                {
                    Ref<Texture2D> existingImage = m_Specification.ExistingImages[attachmentIndex];
                    PG_ASSERT(Utils::IsDepthFormat(existingImage->GetFormat()), "Trying to attach a non-depth image to a depth attachment");
                    m_DepthAttachmentImage = existingImage;
                }
                else
                {
                    TextureSpecification& spec = m_DepthAttachmentImage->GetTextureSpecification();
                    spec.Width = m_Width;
                    spec.Height = m_Height;
                    m_DepthAttachmentImage->Invalidate();
                }

                VkAttachmentDescription& attachmentDescription = attachmentDescriptions.emplace_back();
                attachmentDescription.flags = 0;
                attachmentDescription.format = Utils::GetVulkanImageFormat(attachmentSpec.Format);
                attachmentDescription.samples = Utils::GetSamplerCount(m_Specification.Samples);
                attachmentDescription.loadOp = Utils::GetVulkanAttachmentLoadOp(m_Specification, attachmentSpec);
                // NOTE: If we are sampling it need to be store otherwise DONT_CARE
                attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachmentDescription.initialLayout = attachmentDescription.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                // This is always hit since we can set VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL only if we have separateDepthStencilLayout feature enabled
                if (true || attachmentSpec.Format == ImageFormat::DEPTH24STENCIL8 || attachmentSpec.Format == ImageFormat::DEPTH32FSTENCIL8UINT)
                {
                    VkImageLayout finalLayout = attachmentSpec.Sampled == AttachmentPassThroughUsage::Sampled ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    attachmentDescription.finalLayout = finalLayout;
                    depthAttachmentReference = {
                        .attachment = attachmentIndex,
                        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                    };
                }
                else
                {
                    VkImageLayout finalLayout = attachmentSpec.Sampled == AttachmentPassThroughUsage::Sampled ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                    attachmentDescription.finalLayout = finalLayout;
                    depthAttachmentReference = {
                        .attachment = attachmentIndex,
                        .layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
                    };
                }

                if (m_Specification.ClearDepthOnLoad)
                    m_ClearValues.emplace_back(VkClearValue{
                        .depthStencil = { m_Specification.DepthClearValue, 0 }
                    });
            }
            else
            {
                if (m_Specification.ExistingImages.contains(attachmentIndex))
                {
                    Ref<Texture2D> existingImage = m_Specification.ExistingImages[attachmentIndex];
                    PG_ASSERT(!Utils::IsDepthFormat(existingImage->GetFormat()), "Trying to attach a non-color image to a color attachment");
                    m_ColorAttachmentImages[attachmentIndex] = existingImage;
                }
                else
                {
                    Ref<Texture2D> image = m_ColorAttachmentImages[attachmentIndex];
                    TextureSpecification& spec = image->GetTextureSpecification();
                    spec.Width = m_Width;
                    spec.Height = m_Height;
                    // TODO: Here when we have image layers we should only invalidate if we have one layer
                    image->Invalidate();
                }

                VkAttachmentDescription& attachmentDescription = attachmentDescriptions.emplace_back();
                attachmentDescription.flags = 0;
                attachmentDescription.format = Utils::GetVulkanImageFormat(attachmentSpec.Format);
                attachmentDescription.samples = Utils::GetSamplerCount(m_Specification.Samples);
                attachmentDescription.loadOp = Utils::GetVulkanAttachmentLoadOp(m_Specification, attachmentSpec);
                // NOTE: If we are sampling it need to be store otherwise DONT_CARE
                attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachmentDescription.initialLayout = attachmentDescription.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                VkImageLayout finalLayout = attachmentSpec.Sampled == AttachmentPassThroughUsage::Sampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachmentDescription.finalLayout = finalLayout;

                if (m_Specification.ClearColorOnLoad)
                {
                    m_ClearValues.emplace_back(VkClearValue{
                        .color = {
                            m_Specification.ClearColor.r,
                            m_Specification.ClearColor.g,
                            m_Specification.ClearColor.b,
                            m_Specification.ClearColor.a
                        }
                    });
                }

                colorAttachmentReferences.emplace_back(VkAttachmentReference{ attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
            }

            attachmentIndex++;
        }

        VkSubpassDescription subpassDesc = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size()),
            .pColorAttachments = colorAttachmentReferences.data(),
        };
        if (m_DepthAttachmentImage)
            subpassDesc.pDepthStencilAttachment = &depthAttachmentReference;

        // Use subpass dependencies for layout transitions...
        std::vector<VkSubpassDependency> dependencies;
        if (m_ColorAttachmentImages.size())
        {
            {
                VkSubpassDependency& dependency = dependencies.emplace_back();
                dependency = VkSubpassDependency{
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,
                    .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
                };
            }

            {
                VkSubpassDependency& dependency = dependencies.emplace_back();
                dependency = VkSubpassDependency{
                    .srcSubpass = 0,
                    .dstSubpass = VK_SUBPASS_EXTERNAL,
                    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
                };
            }
        }

        if (m_DepthAttachmentImage)
        {
            {
                VkSubpassDependency& dependency = dependencies.emplace_back();
                dependency = VkSubpassDependency{
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,
                    .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                    .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
                };
            }

            {
                VkSubpassDependency& dependency = dependencies.emplace_back();
                dependency = VkSubpassDependency{
                    .srcSubpass = 0,
                    .dstSubpass = VK_SUBPASS_EXTERNAL,
                    .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
                };
            }
        }

        VkRenderPassCreateInfo renderPassCI = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size()),
            .pAttachments = attachmentDescriptions.data(),
            .subpassCount = 1,
            .pSubpasses = &subpassDesc,
            .dependencyCount = static_cast<uint32_t>(dependencies.size()),
            .pDependencies = dependencies.data()
        };

        VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &m_VulkanRenderPass));
        VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_RENDER_PASS, fmt::format("{} - Renderpass object", m_Specification.DebugName), m_VulkanRenderPass);

        std::vector<VkImageView> attachmentImageViews(m_ColorAttachmentImages.size());
        for (uint32_t i = 0; i < m_ColorAttachmentImages.size(); i++)
        {
            Ref<Texture2D> image = m_ColorAttachmentImages[i];
            attachmentImageViews[i] = image->GetVulkanImageView();
            PG_ASSERT(attachmentImageViews[i], "");
        }

        if (m_DepthAttachmentImage)
        {
            attachmentImageViews.emplace_back(m_DepthAttachmentImage->GetVulkanImageView());
            PG_ASSERT(attachmentImageViews.back(), "");
        }

        VkFramebufferCreateInfo framebufferCI = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = m_VulkanRenderPass,
            .attachmentCount = static_cast<uint32_t>(attachmentImageViews.size()),
            .pAttachments = attachmentImageViews.data(),
            .width = m_Width,
            .height = m_Height,
            .layers = 1
        };

        VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferCI, nullptr, &m_VulkanFramebuffer));
        VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_FRAMEBUFFER, m_Specification.DebugName, m_VulkanFramebuffer);
    }

    void Framebuffer::Resize(uint32_t width, uint32_t height, bool forceRecreate)
    {
        if (!forceRecreate && (m_Width == width && m_Height == height))
            return;

        m_Width = width;
        m_Height = height;

        if (!m_Specification.SwapchainTarget)
            Invalidate();
        else
        {
            SwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();
            m_VulkanRenderPass = swapChain.GetRenderPass();

            m_ClearValues.clear();
            m_ClearValues.emplace_back().color = { 0.0f, 0.0f, 0.0f, 1.0f };
        }
    }

    void Framebuffer::Release()
    {
        if (m_VulkanFramebuffer)
        {
            Renderer::SubmitReseourceFree([framebuffer = m_VulkanFramebuffer, renderpass = m_VulkanRenderPass]()
            {
                VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
                vkDestroyFramebuffer(device, framebuffer, nullptr);
                vkDestroyRenderPass(device, renderpass, nullptr);
            });

            uint32_t attachmentIndex = 0;
            for (Ref<Texture2D> image : m_ColorAttachmentImages)
            {
                // TODO: A bug will be here when we have image layers
                image->Release();
                attachmentIndex++;
            }

            if (m_DepthAttachmentImage)
                m_DepthAttachmentImage->Release();
        }
    }

}