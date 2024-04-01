#include "vkPch.h"
#include "Framebuffer.h"

#include "Renderer.h"

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

    namespace Utils {

        inline VkAttachmentLoadOp GetVulkanAttachmentLoadOp(const FramebufferSpecification& spec, const FramebufferTextureSpecification& attachmentSpec)
        {
            if (attachmentSpec.LoadOp == AttachmentLoadOp::Inherit)
            {
                if (Utils::IsDepthFormat(attachmentSpec.Format))
                {
                    if (spec.Samples > 1)
                    {
                        return spec.ClearDepthOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                    }

                    return spec.ClearDepthOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
                }

                if (spec.Samples > 1)
                {
                    return spec.ClearColorOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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
                    .DebugName = fmt::format("{} - DepthAttachment{}", m_Specification.DebugName.empty() ? "Unnamed FB" : m_Specification.DebugName, attachmentIndex),
                    .Width = static_cast<uint32_t>(m_Width * m_Specification.Scale),
                    .Height = static_cast<uint32_t>(m_Height * m_Specification.Scale),
                    .Format = attachmentSpec.Format,
                    .Usage = ImageUsage::Attachment,
                    .Samples = m_Specification.Samples,
                    .Trasnfer = m_Specification.Transfer
                };
                m_DepthAttachmentImage = Texture2D::CreateNull(spec);

                if (m_Specification.Samples > 1)
                {
                    spec.DebugName = fmt::format("{} - ResolveDepthAttachment{}", m_Specification.DebugName.empty() ? "Unamed FB" : m_Specification.DebugName, attachmentIndex),
                    spec.Samples = 1;
                    m_DepthResolveImage = Texture2D::CreateNull(spec);
                }
            }
            else
            {
                TextureSpecification spec = {
                    .DebugName = fmt::format("{} - ColorAttachment{}", m_Specification.DebugName.empty() ? "Unnamed FB" : m_Specification.DebugName, attachmentIndex),
                    .Width = static_cast<uint32_t>(m_Width * m_Specification.Scale),
                    .Height = static_cast<uint32_t>(m_Height * m_Specification.Scale),
                    .Format = attachmentSpec.Format,
                    .Usage = ImageUsage::Attachment,
                    .Samples = m_Specification.Samples,
                    .Trasnfer = m_Specification.Transfer
                };
                m_ColorAttachmentImages.emplace_back(Texture2D::CreateNull(spec));

                if (m_Specification.Samples > 1)
                {
                    spec.DebugName = fmt::format("{} - ResolveColorAttachment{}", m_Specification.DebugName.empty() ? "Unnamed FB" : m_Specification.DebugName, attachmentIndex),
                    spec.Samples = 1;
                    m_ColorResolveImages.emplace_back(Texture2D::CreateNull(spec));
                }
            }

            ++attachmentIndex;
        }

        VKPG_ASSERT(spec.Attachments.Attachments.size());
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

        std::vector<VkAttachmentDescription2> attachmentDescriptions;

        std::vector<VkAttachmentReference2> colorAttachmentReferences;
        VkAttachmentReference2 depthAttachmentReference;

        uint32_t attachmentIndex = 0;
        for (const auto& attachmentSpec : m_Specification.Attachments.Attachments)
        {
            if (Utils::IsDepthFormat(attachmentSpec.Format))
            {
                if (m_Specification.ExistingImages.contains(attachmentIndex))
                {
                    Ref<Texture2D> existingImage = m_Specification.ExistingImages[attachmentIndex];
                    VKPG_VERIFY(Utils::IsDepthFormat(existingImage->GetFormat()), "Trying to attach a non-depth image to a depth attachment");
                    m_DepthAttachmentImage = existingImage;
                }
                else
                {
                    TextureSpecification& spec = m_DepthAttachmentImage->GetTextureSpecification();
                    spec.Width = m_Width;
                    spec.Height = m_Height;
                    m_DepthAttachmentImage->Invalidate();
                }

                VkAttachmentDescription2& attachmentDescription = attachmentDescriptions.emplace_back();
                attachmentDescription.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
                attachmentDescription.pNext = nullptr;
                attachmentDescription.flags = 0;
                attachmentDescription.format = Utils::GetVulkanImageFormat(attachmentSpec.Format);
                attachmentDescription.samples = Utils::GetSamplerCount(m_Specification.Samples);
                attachmentDescription.loadOp = Utils::GetVulkanAttachmentLoadOp(m_Specification, attachmentSpec);
                // NOTE: If we are sampling it need to be store otherwise DONT_CARE
                attachmentDescription.storeOp = m_Specification.Samples > 1 ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
                attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachmentDescription.initialLayout = attachmentDescription.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                // This is always hit since we can set VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL only if we have separateDepthStencilLayout feature enabled
                if (true || attachmentSpec.Format == ImageFormat::DEPTH24STENCIL8 || attachmentSpec.Format == ImageFormat::DEPTH32FSTENCIL8UINT)
                {
                    VkImageLayout finalLayout = attachmentSpec.Sampled == AttachmentPassThroughUsage::Sampled ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    attachmentDescription.finalLayout = m_Specification.Samples > 1 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : finalLayout;
                    // We override here in case we have a sampled image since a sampled image can not be read from directly

                    depthAttachmentReference = {
                        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                        .pNext = nullptr,
                        .attachment = attachmentIndex,
                        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                    };
                }
                else
                {
                    VkImageLayout finalLayout = attachmentSpec.Sampled == AttachmentPassThroughUsage::Sampled ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                    attachmentDescription.finalLayout = m_Specification.Samples > 1 ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : finalLayout;
                    // We override here in case we have a sampled image since a sampled image can not be read from directly

                    depthAttachmentReference = {
                        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                        .pNext = nullptr,
                        .attachment = attachmentIndex,
                        .layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
                    };
                }

                if (m_Specification.ClearDepthOnLoad)
                {
                    m_ClearValues.emplace_back(VkClearValue{
                        .depthStencil = { m_Specification.DepthClearValue, 0 }
                    });
                }
            }
            else
            {
                if (m_Specification.ExistingImages.contains(attachmentIndex))
                {
                    Ref<Texture2D> existingImage = m_Specification.ExistingImages[attachmentIndex];
                    VKPG_ASSERT(!Utils::IsDepthFormat(existingImage->GetFormat()), "Trying to attach a non-color image to a color attachment");
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

                VkAttachmentDescription2& attachmentDescription = attachmentDescriptions.emplace_back();
                attachmentDescription.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
                attachmentDescription.pNext = nullptr;
                attachmentDescription.flags = 0;
                attachmentDescription.format = Utils::GetVulkanImageFormat(attachmentSpec.Format);
                attachmentDescription.samples = Utils::GetSamplerCount(m_Specification.Samples);
                attachmentDescription.loadOp = Utils::GetVulkanAttachmentLoadOp(m_Specification, attachmentSpec);
                // NOTE: If we are sampling it need to be store otherwise DONT_CARE
                attachmentDescription.storeOp = m_Specification.Samples > 1 ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
                attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachmentDescription.initialLayout = attachmentDescription.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                VkImageLayout finalLayout = attachmentSpec.Sampled == AttachmentPassThroughUsage::Sampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachmentDescription.finalLayout = m_Specification.Samples > 1 ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : finalLayout;

                colorAttachmentReferences.emplace_back(VkAttachmentReference2{
                    .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                    .pNext = nullptr,
                    .attachment = attachmentIndex,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                });

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
            }

            attachmentIndex++;
        }

        // In case of multisampling we need a resolve image
        VkSubpassDescriptionDepthStencilResolve depthStencilResolve;
        if (m_Specification.Samples > 1)
        {
            uint32_t attachmentDescriptionIndex;
            for (attachmentDescriptionIndex = 0; attachmentDescriptionIndex < m_ColorResolveImages.size(); attachmentDescriptionIndex++)
            {
                // Invalidate the image...
                Ref<Texture2D> image = m_ColorResolveImages[attachmentDescriptionIndex];
                TextureSpecification& spec = image->GetTextureSpecification();
                spec.Width = m_Width;
                spec.Height = m_Height;
                image->Invalidate();

                VkAttachmentDescription2& resolveAttachmentDescription = attachmentDescriptions.emplace_back();
                resolveAttachmentDescription = VkAttachmentDescription2{
                    .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                    .pNext = nullptr,
                    .flags = 0,
                    .format = Utils::GetVulkanImageFormat(image->GetFormat()),
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = m_Specification.Attachments.Attachments[attachmentDescriptionIndex].Sampled == AttachmentPassThroughUsage::Sampled ? 
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                                                              : 
                                                attachmentDescriptions[attachmentDescriptionIndex].finalLayout
                };

                // attachmentIndex here should have been incremented
                colorAttachmentReferences.emplace_back(VkAttachmentReference2{
                    .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                    .pNext = nullptr,
                    .attachment = attachmentIndex,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                });

                attachmentIndex++;
            }


            if (m_DepthAttachmentImage)
            {
                // Invalidate the image...
                TextureSpecification& spec = m_DepthResolveImage->GetTextureSpecification();
                spec.Width = m_Width;
                spec.Height = m_Height;
                m_DepthResolveImage->Invalidate();

                VkAttachmentDescription2& depthResolveAttachmentDesc = attachmentDescriptions.emplace_back();
                depthResolveAttachmentDesc = VkAttachmentDescription2{
                    .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                    .pNext = nullptr,
                    .flags = 0,
                    .format = Utils::GetVulkanImageFormat(m_DepthResolveImage->GetFormat()),
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = m_Specification.Attachments.Attachments[attachmentDescriptionIndex].Sampled == AttachmentPassThroughUsage::Sampled ?
                                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL                                                           :
                                                attachmentDescriptions[attachmentDescriptionIndex].finalLayout
                };

                depthStencilResolve = {
                    .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE,
                    .pNext = nullptr,
                    .depthResolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
                    .stencilResolveMode = VK_RESOLVE_MODE_NONE,
                    .pDepthStencilResolveAttachment = [](VkAttachmentReference2&& temp) -> VkAttachmentReference2* { return &temp; }(VkAttachmentReference2{
                        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                        .pNext = nullptr,
                        .attachment = attachmentIndex,
                        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                    })
                };
            }
        }

        VkSubpassDescription2 subPassDesc = {
            .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
            .pNext = (m_Specification.Samples > 1 && m_DepthAttachmentImage != nullptr) ? &depthStencilResolve : nullptr,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size() - m_ColorResolveImages.size()),
            .pColorAttachments = colorAttachmentReferences.data(),
        };
        if (m_DepthAttachmentImage)
            subPassDesc.pDepthStencilAttachment = &depthAttachmentReference;
        if (m_Specification.Samples > 1)
            subPassDesc.pResolveAttachments = (colorAttachmentReferences.data() + m_ColorAttachmentImages.size());

        // Use subpass dependencies for layout transitions...
        std::vector<VkSubpassDependency2> dependencies;
        dependencies.reserve((m_ColorAttachmentImages.size() ? 2 : 0) + (m_DepthAttachmentImage ? 2 : 0));
        if (m_ColorAttachmentImages.size())
        {
            {
                VkSubpassDependency2& dependency = dependencies.emplace_back();
                dependency = VkSubpassDependency2{
                    .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
                    .pNext = nullptr,
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
                VkSubpassDependency2& dependency = dependencies.emplace_back();
                dependency = VkSubpassDependency2{
                    .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
                    .pNext = nullptr,
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
                VkSubpassDependency2& dependency = dependencies.emplace_back();
                dependency = VkSubpassDependency2{
                    .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
                    .pNext = nullptr,
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
                VkSubpassDependency2& dependency = dependencies.emplace_back();
                dependency = VkSubpassDependency2{
                    .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
                    .pNext = nullptr,
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

        VkRenderPassCreateInfo2 renderPassCI = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
            .attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size()),
            .pAttachments = attachmentDescriptions.data(),
            .subpassCount = 1,
            .pSubpasses = &subPassDesc,
            .dependencyCount = static_cast<uint32_t>(dependencies.size()),
            .pDependencies = dependencies.data()
        };

        VK_CHECK_RESULT(vkCreateRenderPass2(device, &renderPassCI, nullptr, &m_VulkanRenderPass));
        VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_RENDER_PASS, fmt::format("{} - Renderpass object", m_Specification.DebugName), m_VulkanRenderPass);

        uint32_t numberOfAttachmentImages = static_cast<uint32_t>(m_ColorAttachmentImages.size() + ((m_DepthAttachmentImage != nullptr) ? 1 : 0));
        uint32_t numberOfResolveImages = static_cast<uint32_t>(m_ColorResolveImages.size() + ((m_DepthResolveImage != nullptr) ? 1 : 0));
        std::vector<VkImageView> attachmentImageViews(numberOfAttachmentImages + numberOfResolveImages);

        uint32_t attachementViewsIndex = 0;
        for (; attachementViewsIndex < m_ColorAttachmentImages.size(); attachementViewsIndex++)
        {
            Ref<Texture2D> image = m_ColorAttachmentImages[attachementViewsIndex];
            attachmentImageViews[attachementViewsIndex] = image->GetVulkanImageView();
            VKPG_ASSERT(attachmentImageViews[attachementViewsIndex]);
        }

        if (m_DepthAttachmentImage)
        {
            attachmentImageViews[attachementViewsIndex] = m_DepthAttachmentImage->GetVulkanImageView();
            VKPG_ASSERT(attachmentImageViews[attachementViewsIndex]);

            attachementViewsIndex++;
        }

        if (m_Specification.Samples > 1)
        {
            for (uint32_t i = 0; i < m_ColorResolveImages.size(); i++, attachementViewsIndex++)
            {
                attachmentImageViews[attachementViewsIndex] = m_ColorResolveImages[i]->GetVulkanImageView();
                VKPG_ASSERT(attachmentImageViews[attachementViewsIndex]);
            }

            if (m_DepthAttachmentImage)
            {
                attachmentImageViews[attachementViewsIndex] = m_DepthResolveImage->GetVulkanImageView();
                VKPG_ASSERT(attachmentImageViews[attachementViewsIndex]);
            }
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

        m_Width = static_cast<uint32_t>(width * m_Specification.Scale);
        m_Height = static_cast<uint32_t>(height * m_Specification.Scale);

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
                // Do not free the image if we do not own it...
                if (m_Specification.ExistingImages.contains(attachmentIndex))
                    continue;
                // TODO: A bug will be here when we have image layers
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
        }
    }

}