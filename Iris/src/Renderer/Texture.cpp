#include "IrisPCH.h"
#include "Texture.h"

#include "Renderer.h"
#include "Renderer/Core/Vulkan.h"
#include "Utils/TextureImporter.h"

/*
 * NOTE: If we ever get validation layers about something related to iamge layouts and pipeline stage bits/masks or aspectMasks directly check
 * whatever is being set to the mentioned states since it could be hard coded instead of decided according to the specification
 */

namespace Iris {

    namespace Utils {

        static constexpr void ValidateSpecification(const TextureSpecification& spec)
        {
            IR_ASSERT(spec.Width > 0 && spec.Height > 0 && spec.Width < 65536 && spec.Height < 65536);
        }

        static constexpr VkSamplerAddressMode GetVulkanSamplerWrap(TextureWrap wrap)
        {
            switch (wrap)
            {
                case TextureWrap::Clamp:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                case TextureWrap::Repeat:  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            }

            IR_ASSERT(false);
            return (VkSamplerAddressMode)0;
        }

        static constexpr VkFilter GetVulkanSamplerFilter(TextureFilter filter)
        {
            switch (filter)
            {
                case TextureFilter::Linear:   return VK_FILTER_LINEAR;
                case TextureFilter::Nearest:  return VK_FILTER_NEAREST;
            }

            IR_ASSERT(false);
            return (VkFilter)0;
        }

        static constexpr std::size_t GetMemorySize(ImageFormat format, uint32_t width, uint32_t height)
        {
            switch (format)
            {
                case ImageFormat::R8UI:                     return width * height;
                case ImageFormat::R8UN:                     return width * height;
                case ImageFormat::R16UI:                    return width * height * sizeof(uint16_t);
                case ImageFormat::R32F:                     return width * height * sizeof(float);
                case ImageFormat::RG16F:                    return width * height * 2 * sizeof(uint16_t);
                case ImageFormat::RG32F:                    return width * height * 2 * sizeof(float);
                case ImageFormat::RGBA:                     return width * height * 4;
                case ImageFormat::RGBA32F:                  return width * height * 4 * sizeof(float);
                case ImageFormat::B10R11G11UF:              return width * height * sizeof(float);
                case ImageFormat::SRGB:                     return width * height * 3;
                case ImageFormat::SRGBA:                    return width * height * 4;
            }

            IR_ASSERT(false);
            return 0;
        }

    }

    Ref<Texture2D> Texture2D::CreateNull(const TextureSpecification& spec)
    {
        return CreateRef<Texture2D>(spec);
    }

    Ref<Texture2D> Texture2D::Create(const TextureSpecification& spec, const std::string& filePath)
    {
        return CreateRef<Texture2D>(spec, filePath);
    }

    Ref<Texture2D> Texture2D::Create(const TextureSpecification& spec, Buffer imageData)
    {
        return CreateRef<Texture2D>(spec, imageData);
    }

    Texture2D::Texture2D(const TextureSpecification& spec)
        : m_Specification(spec)
    {
        IR_VERIFY(m_Specification.Width > 0 && m_Specification.Height > 0);
    }

    Texture2D::Texture2D(const TextureSpecification& spec, const std::string& filePath)
        : m_Specification(spec), m_AssetPath(filePath)
    {
        Utils::ValidateSpecification(m_Specification);

        // Load image from file
        m_ImageData = Utils::TextureImporter::LoadImageFromFile(m_AssetPath, m_Specification.Format, m_Specification.Width, m_Specification.Height);
        if (!m_ImageData)
        {
            m_ImageData = Utils::TextureImporter::LoadImageFromFile("assets/textures/cap.jpg", m_Specification.Format, m_Specification.Width, m_Specification.Height);
        }

        m_Specification.Mips = m_Specification.GenerateMips ? GetMipLevelCount() : 1;

        IR_VERIFY(m_Specification.Format != ImageFormat::None);

        Invalidate();
    }

    Texture2D::Texture2D(const TextureSpecification& spec, Buffer imageData)
        : m_Specification(spec), m_AssetPath("")
    {
        // Load image from memory
        if (m_Specification.Height == 0)
        {
            m_ImageData = Utils::TextureImporter::LoadImageFromMemory(imageData, m_Specification.Format, m_Specification.Width, m_Specification.Height);
            if (!m_ImageData)
            {
                constexpr uint32_t errorTextureData = 0xff0000ff;
                m_ImageData = Buffer((uint8_t*)&errorTextureData, sizeof(uint32_t));
            }

            Utils::ValidateSpecification(m_Specification);
        }
        else if (imageData)
        {
            Utils::ValidateSpecification(m_Specification);
            uint32_t size = (uint32_t)Utils::GetMemorySize(m_Specification.Format, m_Specification.Width, m_Specification.Height);
            m_ImageData = Buffer::Copy(imageData);
        }
        else // Fallback
        {
            if (m_Specification.Usage != ImageUsage::Attachment)
            {
                Utils::ValidateSpecification(m_Specification);
                uint32_t size = (uint32_t)Utils::GetMemorySize(m_Specification.Format, m_Specification.Width, m_Specification.Height);
                m_ImageData.Allocate(size);
                std::memset(m_ImageData.Data, 0, m_ImageData.Size);
            }
        }
        
        m_Specification.Mips = m_Specification.GenerateMips ? GetMipLevelCount() : 1;

        IR_VERIFY(m_Specification.Format != ImageFormat::None);

        Invalidate();
    }

    Texture2D::~Texture2D()
    {
        Release();
    }

    void Texture2D::Invalidate()
    {
        IR_ASSERT(m_Specification.Width > 0 && m_Specification.Height > 0);

        // Try to release all the resources before starting to create since Invalidate could be called from `Texture2D::Resize`
        Release();

        uint32_t mipCount = m_Specification.GenerateMips ? GetMipLevelCount() : 1;

        Ref<VulkanDevice> logicalDevice = RendererContext::GetCurrentDevice();
        VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
        VulkanAllocator allocator("Texture2D");

        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        if (m_Specification.Usage == ImageUsage::Attachment)
        {
            if (Utils::IsDepthFormat(m_Specification.Format))
                usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            else
                usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        // We set it as a transfer src and dst for textures to generate mips
        // Also for attachments in case we want to blit them or copy to host buffers
        if (m_Specification.Trasnfer || m_Specification.Usage == ImageUsage::Texture || m_Specification.Usage == ImageUsage::Attachment)
        {
            usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        // TODO: Storage Images will be their own thing
        // if (m_Specification.Usage == ImageUsage::Storage)
        // {
        //     usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        // }

        VkImageAspectFlags aspectMask = Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        if (m_Specification.Format == ImageFormat::DEPTH32FSTENCIL8UINT || m_Specification.Format == ImageFormat::DEPTH24STENCIL8)
            aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        VkImageCreateInfo imageCI = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = Utils::GetVulkanImageFormat(m_Specification.Format),
            .extent = { .width = m_Specification.Width, .height = m_Specification.Height, .depth = 1u },
            .mipLevels = mipCount,
            .arrayLayers = 1, // TODO (whether the texture is an array)
            .samples = Utils::GetSamplerCount(m_Specification.Samples),
            .tiling = VK_IMAGE_TILING_OPTIMAL, // NOTE: This should be decided on a boolean whether the image usage is HostRead or not when we have that
            // .tiling = m_Specification.Usage == ImageUsage::HostRead ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED // Not usable by gpu and first transition will discard texels
        };

        // 1. Create image object
        VkDeviceSize gpuAllocationSize = 0;
        VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_GPU_ONLY; // NOTE: This should be decided on a boolean whether the image usage is HostRead or not when we have that
        // VmaMemoryUsage memUsage = m_Specification.Usage == ImageUsage::HostRead ? VMA_MEMORY_USAGE_GPU_TO_CPU : VMA_MEMORY_USAGE_GPU_ONLY;
        m_MemoryAllocation = allocator.AllocateImage(&imageCI, memUsage, &m_Image, &gpuAllocationSize);
        VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE, m_Specification.DebugName, m_Image);

        if (m_ImageData && m_Specification.Usage != ImageUsage::Attachment)
        {
            VkBuffer stagingBuffer;
            VkBufferCreateInfo stagingBufferCI = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = m_ImageData.Size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            VmaAllocation stagingBufferAlloc = allocator.AllocateBuffer(&stagingBufferCI, VMA_MEMORY_USAGE_CPU_TO_GPU, &stagingBuffer);

            uint8_t* dstData = allocator.MapMemory<uint8_t>(stagingBufferAlloc);
            std::memcpy(dstData, m_ImageData.Data, m_ImageData.Size);
            allocator.UnmapMemory(stagingBufferAlloc);

            // At this point the image data is on the gpu in the staging buffer so we dont need it on the cpu side anymore so we release
            Utils::TextureImporter::FreeImageMemory(m_ImageData.Data);
            m_ImageData.Size = 0;

            /*
             * Layout Transitions and data copy
             * Transition image layout from IMAGE_LAYOUT_UNDEFINED -> IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
             * First Transition to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
             * Pipeline Barriers are used to synchronize access to resources... 
             * They ensure that a resource finished doing something before doing something else
             * *** Certain edge cases to handle with src/dstAccessMask ***
             * if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
             * {
             *      barrier.srcAccessMask = 0;
             *      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
             *      sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
             *      destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
             * }
             * else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
             * {
             *      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
             *      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
             *      sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
             *      destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
             * }
             */

            VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

            // https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization/ (ImageMemoryBarriers)
            // https://gpuopen.com/learn/vulkan-barriers-explained/ (TOP_OF_PIPE and BOTTOM_OF_PIPE)

            Renderer::InsertImageMemoryBarrier(
                commandBuffer,
                m_Image,
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Wait for nothing
                VK_PIPELINE_STAGE_TRANSFER_BIT, // Unblock transfer operations after this transition is done
                { 
                    .aspectMask = aspectMask, 
                    .baseMipLevel = 0,
                    .levelCount = 1, // Transition only the original image to TRANSFER_DST_OPTIMAL. We should not touch the mips here...
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            );

            // Copy image
            VkBufferImageCopy copyRegion = {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                    .aspectMask = aspectMask,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .imageOffset = { .x = 0, .y = 0, .z = 0 },
                .imageExtent = { .width = m_Specification.Width, .height = m_Specification.Height, .depth = 1u }
            };

            vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            // NOTE: When we have HostRead usage we need to handle the final layout since it should be a transfer related
            VkImageLayout finalImageLayout;
            if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8 || m_Specification.Format == ImageFormat::DEPTH32F || m_Specification.Format == ImageFormat::DEPTH32FSTENCIL8UINT)
                finalImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            else
                finalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Conditionally transtition if we have mips or not
            if (mipCount > 1)
            {
                Renderer::InsertImageMemoryBarrier(
                    commandBuffer,
                    m_Image,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    // Here we transition only the original image to a transfer src since the generate mips starts from the first mip
                    // All the other mips (which yet do not exist) are still in IMAGE_LAYOUT_UNDEFINED untill now...
                    { .aspectMask = aspectMask, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
                );
            }
            else
            {
                // Second Layout Transition
                // NOTE: We should take care of the dstStageMask when we have HostRead Images, the stuff to block should be trasnfer stage related 
                Renderer::InsertImageMemoryBarrier(
                    commandBuffer, 
                    m_Image, 
                    VK_ACCESS_TRANSFER_WRITE_BIT, 
                    VK_ACCESS_SHADER_READ_BIT, 
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    finalImageLayout,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, // Wait for transfer opration to finish
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // Unblock all fragment shader operations after this transfer is done
                    { .aspectMask = aspectMask, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
                );
            }

            logicalDevice->FlushCommandBuffer(commandBuffer);
            allocator.DestroyBuffer(stagingBufferAlloc, stagingBuffer);
        }
        // else
        // {
        //     // We should generally never not have data unless the texture is an attachment since not having data is only possible in case of
        //     // storage iamges which will be separated into their own thing...
        // }

        // VkImageAspectFlags aspectMask = Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageViewCreateInfo imageViewCI = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_Image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = Utils::GetVulkanImageFormat(m_Specification.Format),
            .components = { .r = VK_COMPONENT_SWIZZLE_R, .g = VK_COMPONENT_SWIZZLE_G, .b = VK_COMPONENT_SWIZZLE_B, .a = VK_COMPONENT_SWIZZLE_A },
            .subresourceRange = {
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = mipCount,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &m_ImageView));
        VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, fmt::format("{} Image View", m_Specification.DebugName), m_ImageView);

        // Create sampler for the texture that defines how the image is filtered and applies transformation for the final pixel color before it is
        // ready to be retrieved by the shader
        // Sampler do not reference an image like in old APIs, rather they are their own standalone objects and can be used for multiple types of
        // images however in playground each texture could own its own sampler
        // Attachment images will have samplers which allows the user to sample from them in another pass if they want
        if (m_Specification.CreateSampler)
        {
            Ref<VulkanPhysicalDevice> physicalDevice = RendererContext::GetCurrentDevice()->GetPhysicalDevice();
            bool enableAnisotropy = physicalDevice->GetPhysicalDeviceFeatures().samplerAnisotropy;
            float samplerAnisotorpy = enableAnisotropy ? physicalDevice->GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy : 1.0f;

            VkSamplerCreateInfo samplerCI = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = Utils::GetVulkanSamplerFilter(m_Specification.FilterMode),
                .minFilter = Utils::GetVulkanSamplerFilter(m_Specification.FilterMode),
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .addressModeU = Utils::GetVulkanSamplerWrap(m_Specification.WrapMode),
                .addressModeV = Utils::GetVulkanSamplerWrap(m_Specification.WrapMode),
                .addressModeW = Utils::GetVulkanSamplerWrap(m_Specification.WrapMode),
                .mipLodBias = 0.0f,
                .anisotropyEnable = enableAnisotropy,
                .maxAnisotropy = samplerAnisotorpy,
                .compareEnable = VK_FALSE,
                .compareOp = VK_COMPARE_OP_NEVER,
                .minLod = 0.0f,
                .maxLod = static_cast<float>(mipCount),
                .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                // Use coordinate system [0, 1) in order to access texels in the texture instead of [0, textureWidth)
                .unnormalizedCoordinates = VK_FALSE
            };

            VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &m_Sampler));
        }
        
        // Update image descriptor info
        VkImageLayout finalImageLayout;
        if (m_Specification.Format == ImageFormat::DEPTH24STENCIL8 || m_Specification.Format == ImageFormat::DEPTH32F || m_Specification.Format == ImageFormat::DEPTH32FSTENCIL8UINT)
            finalImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        else
            finalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        m_DescriptorInfo = VkDescriptorImageInfo{
            .sampler = m_Sampler,
            .imageView = m_ImageView,
            .imageLayout = finalImageLayout
        };

        if (m_Specification.GenerateMips && mipCount > 1)
            GenerateMips();
    }

    void Texture2D::Resize(uint32_t width, uint32_t height)
    {
        m_Specification.Width = width;
        m_Specification.Height = height;

        Invalidate();
    }

    void Texture2D::GenerateMips()
    {
        Ref<VulkanDevice> logicalDevice = RendererContext::GetCurrentDevice();
        VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

        const uint32_t mipCount = GetMipLevelCount();

        VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

        for (uint32_t i = 1; i < mipCount; i++) // Loop starts at 1 since mip 0 is the original image
        {
            VkImageSubresourceRange mipSubResourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = i,
                .levelCount = 1,
                .layerCount = 1
            };

            Renderer::InsertImageMemoryBarrier(
                commandBuffer,
                m_Image,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, // Before the blit we will write and after the blit we will read
                VK_IMAGE_LAYOUT_UNDEFINED, // Here it is UNDEFINED since when we copy the 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                mipSubResourceRange
            );

            VkImageBlit imageBlit = {
                // Source
                .srcSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = i - 1,
                    .layerCount = 1
                },
                // Right shifting is basically division by 2
                .srcOffsets = { { 0, 0, 0 }, { int32_t(m_Specification.Width >> (i - 1)), int32_t(m_Specification.Height >> (i - 1)), 1 } },

                // Destination
                .dstSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = i,
                    .layerCount = 1
                },
                .dstOffsets = { { 0, 0, 0 }, { int32_t(m_Specification.Width >> i), int32_t(m_Specification.Height >> i), 1 } },
            };

            vkCmdBlitImage(
                commandBuffer,
                m_Image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_Image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &imageBlit,
                Utils::GetVulkanSamplerFilter(m_Specification.FilterMode)
            );

            // Transition the current mip back from dst to src for the next iteration
            Renderer::InsertImageMemoryBarrier(
                commandBuffer,
                m_Image,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                mipSubResourceRange
            );
        }
        
        Renderer::InsertImageMemoryBarrier(
            commandBuffer,
            m_Image,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = mipCount, .layerCount = 1 }
        );

        logicalDevice->FlushCommandBuffer(commandBuffer);
    }

    void Texture2D::Release()
    {
        // Does nothing to m_ImageData since that should have been released by `Invalidate`
        if (m_Image == nullptr)
            return;

        // TODO: Once we have per mip image views and per mip image layers views we need to also destroy those

        Renderer::SubmitReseourceFree([image = m_Image, imageView = m_ImageView, sampler = m_Sampler, allocation = m_MemoryAllocation]()
        {
            VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
            VulkanAllocator allocator("Texture2D");
            vkDestroySampler(device, sampler, nullptr);
            vkDestroyImageView(device, imageView, nullptr);
            allocator.DestroyImage(allocation, image);
        });

        m_Image = nullptr;
        m_ImageView = nullptr;
        if(m_Specification.CreateSampler)
            m_Sampler = nullptr;
        m_MemoryAllocation = nullptr;
    }

    void Texture2D::CopyToHostBuffer(Buffer& buffer, bool writeMips) const
    {
        // Transition image layout to transfer src then copy to host buffer and transition back to original layout
        Ref<VulkanDevice> logicalDevice = RendererContext::GetCurrentDevice();
        VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
        VulkanAllocator allocator("Texture2D");

        uint32_t mipCount = writeMips && m_Specification.GenerateMips ? GetMipLevelCount() : 1;
        uint32_t mipWidth = m_Specification.Width;
        uint32_t mipHeight = m_Specification.Height;

        // Calculate the bufferSize needed to store all the mips
        uint64_t bufferSize = m_Specification.Width * m_Specification.Height * Utils::GetImageFormatBPP(m_Specification.Format);
        {
            uint32_t tempWidth = mipWidth / 2;
            uint32_t tempHeight = mipHeight / 2;
            for (uint32_t i = 1; i < mipCount; i++)
            {
                bufferSize += tempWidth * tempHeight * Utils::GetImageFormatBPP(m_Specification.Format);
                tempWidth /= 2;
                tempHeight /= 2;
            }
        }

        VkBuffer stagingBuffer;
        VkBufferCreateInfo stagingBufferCI = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = bufferSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        VmaAllocation stagingBufferAllocation = allocator.AllocateBuffer(&stagingBufferCI, VMA_MEMORY_USAGE_CPU_TO_GPU, &stagingBuffer);

        VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

        VkImageAspectFlags aspectMask = Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        if (m_Specification.Format == ImageFormat::DEPTH32FSTENCIL8UINT || m_Specification.Format == ImageFormat::DEPTH24STENCIL8)
            aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        // options to be determined by the Usage in the specification
        // if Texture: Wait for nothing
        // if Attachment: Wait for color attachment
        // if Attachment and Depth format: Wait for fragment shadar execution
        // if Storage: Will be handled in the storage images separately but it will be to wait for compute shader execution (most probably)
        VkPipelineStageFlags srcPipelineStageMask;
        if (m_Specification.Usage == ImageUsage::Texture) [[unlikely]]
        {
            // Logically this should never be hit since why would you want to copy a texture that is loaded by you?
            srcPipelineStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Wait for nothing if it is a texture.
        }
        else if (m_Specification.Usage == ImageUsage::Attachment && Utils::IsDepthFormat(m_Specification.Format))
        {
            srcPipelineStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // Wait for fragment shader to finish execution so the depth texture is written
        }
        else // if(m_Specification.Usage == ImageUsage::Attachment)
        {
            srcPipelineStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Wait for the color attachment to be outputted
        }

        Renderer::InsertImageMemoryBarrier(
            commandBuffer,
            m_Image,
            0,
            VK_ACCESS_TRANSFER_READ_BIT, // Since we would like to read FROM the image to the buffer
            m_DescriptorInfo.imageLayout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            srcPipelineStageMask,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            {
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = mipCount,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        );

        uint64_t mipDataOffset = 0;
        for (uint32_t mip = 0; mip < mipCount; mip++)
        {
            // Copy image to buffer
            VkBufferImageCopy copyRegion = {
                .bufferOffset = mipDataOffset,
                .imageSubresource = {
                    .aspectMask = aspectMask,
                    .mipLevel = mip,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .imageExtent = { .width = mipWidth, .height = mipHeight, .depth = 1u }
            };

            vkCmdCopyImageToBuffer(commandBuffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &copyRegion);

            uint64_t mipDataSize = mipWidth * mipHeight * Utils::GetImageFormatBPP(m_Specification.Format);
            mipDataOffset += mipDataSize;
            mipWidth /= 2;
            mipHeight /= 2;
        }

        Renderer::InsertImageMemoryBarrier(
            commandBuffer,
            m_Image,
            VK_ACCESS_TRANSFER_READ_BIT,
            0,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_DescriptorInfo.imageLayout,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Do not block any upcoming commands
            {
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = mipCount,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        );

        logicalDevice->FlushCommandBuffer(commandBuffer);

        // Copy to destination host buffer
        uint8_t* srcData = allocator.MapMemory<uint8_t>(stagingBufferAllocation);
        buffer.Allocate(bufferSize);
        std::memcpy(buffer.Data, srcData, bufferSize);
        allocator.UnmapMemory(stagingBufferAllocation);

        allocator.DestroyBuffer(stagingBufferAllocation, stagingBuffer);
    }

    uint32_t Texture2D::GetMipLevelCount() const
    {
        return Utils::CalculateMipCount(m_Specification.Width, m_Specification.Height);
    }

    glm::ivec2 Texture2D::GetMipSize(uint32_t mip) const
    {
        uint32_t width = m_Specification.Width;
        uint32_t height = m_Specification.Height;

        while (mip != 0)
        {
            width /= 2;
            height /= 2;
            --mip;
        }

        return { width, height };
    }

}