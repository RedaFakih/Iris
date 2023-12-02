#pragma once

#include "Core/Base.h"
#include "Core/Buffer.h"
#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/VulkanAllocator.h"

#include <glm/glm.hpp>
#include <glm/gtc/integer.hpp>
#include <vulkan/vulkan.h>

namespace vkPlayground {

	enum class ImageFormat : uint8_t
	{
		None = 0,
		R8UN, // UNORM
		R8UI, // UINT
		R16UI, // UINT
		R32UI, // UINT
		R32F, // FLOAT

		RG8, // UNORM
		RG16F, // FLOAT
		RG32F, // FLOAT

		RGB, // UNORM
		RGBA, // UNORM
		RGBA16F, // FLOAT
		RGBA32F, // FLOAT

		B10R11G11UF, // UFLOAT

		DEPTH32FSTENCIL8UINT, // UINT
		DEPTH32F, // FLOAT
		DEPTH24STENCIL8, // Default device depth format
	};

	enum class TextureWrap : uint8_t
	{
		None = 0,
		Clamp,
		Repeat
	};

	enum class TextureFilter : uint8_t
	{
		None = 0,
		Linear,
		Nearest,
		Cubic
	};

	enum class ImageUsage : uint8_t
	{
		None = 0,
		Texture, // Defualt: Loaded via a filepath or image data
		Attachment, // For Framebuffers
		// NOTE: HostRead // Usefull for read-back images (Mousepicking using IDs texture for example)
	};

	struct TextureSpecification
	{
		std::string DebugName;

		// Gets set internally if there was a filepath given or if the height is 0, otherwise set by user
		uint32_t Width = 1;
		// Gets set internally if there was a filepath given or if the height is 0, otherwise set by user
		uint32_t Height = 1; 

		// Gets set internally if there was a filepath given, otherwise it is specified by user
		ImageFormat Format = ImageFormat::RGBA;
		// Set by User and Also set internally if needed
		ImageUsage Usage = ImageUsage::Texture;

		// Set by user
		bool CreateSampler = true;
		// Set by user
		TextureWrap WrapMode = TextureWrap::Repeat;
		// Set by user
		TextureFilter FilterMode = TextureFilter::Linear;

		// Used for Transfer operations? (Affects the usage of the image)
		bool Trasnfer = false;

		// Set by user. Generate Mipmap
		bool GenerateMips = false;
		// DO NOT SET THIS. This will be determined up on invalidation and is there for debugging purposes.
		uint32_t Mips = 0;

		// TODO: We could store a cache map for per-layer image views and another for per-mip image views and to create them we just loop
		// TODO: on the mipCount and the loop on Layers and just change the subResourceRange for VkImageViewCreateInfo
		// Set by user. Texture might an array useful for shadow mapping
		uint32_t Layers = 1;
	};

	class Texture2D : public RefCountedObject
	{
	public:
		Texture2D(const TextureSpecification& spec, const std::string& filePath);
		Texture2D(const TextureSpecification& spec, Buffer imageData = Buffer());
		~Texture2D();

		[[nodiscard]] static Ref<Texture2D> Create(const TextureSpecification& spec, const std::string& filePath);
		[[nodiscard]] static Ref<Texture2D> Create(const TextureSpecification& spec, Buffer imageData = Buffer());

		void Invalidate();
		void Resize(uint32_t width, uint32_t height);
		void GenerateMips();
		void Release();

		void CopyToHostBuffer(Buffer& buffer, bool writeMips = false) const;

		const std::string& GetAssetPath() const noexcept { return m_AssetPath; }
		ImageFormat GetFormat() const noexcept { return m_Specification.Format; }
		uint32_t GetWidth() const noexcept { return m_Specification.Width; }
		uint32_t GetHeight() const noexcept { return m_Specification.Height; }
		const glm::vec2& GetSize() const noexcept { return { m_Specification.Width, m_Specification.Height }; }

		const VkImage GetVulkanImage() const { return m_Image; }
		const VkImageView GetVulkanImageView() const { return m_ImageView; }
		const VkSampler GetVulkanSampler() const { return m_Sampler; }
		const VmaAllocation GetMemoryAllocation() const { return m_MemoryAllocation; }

		const VkDescriptorImageInfo& GetDescriptorImageInfo() const { return m_DescriptorInfo; }

		TextureSpecification& GetTextureSpecification() { return m_Specification; }
		const TextureSpecification& GetTextureSpecification() const { return m_Specification; }

		uint32_t GetMipLevelCount() const;
		glm::ivec2 GetMipSize(uint32_t mip) const;

	private:
		std::string m_AssetPath;
		TextureSpecification m_Specification;

		// Image info
		VkImage m_Image = nullptr;
		VkImageView m_ImageView = nullptr;
		VkSampler m_Sampler = nullptr;
		VmaAllocation m_MemoryAllocation = nullptr;

		Buffer m_ImageData; // Local storage of the image

		VkDescriptorImageInfo m_DescriptorInfo;
	};

	namespace Utils {

		inline constexpr uint32_t GetImageFormatBPP(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::R8UN:			return 1;
				case ImageFormat::R8UI:			return 1;
				case ImageFormat::R16UI:		return 2;
				case ImageFormat::R32UI:		return 4;
				case ImageFormat::R32F:			return 4;
				case ImageFormat::RG8:			return 1 * 2;
				case ImageFormat::RG16F:		return 2 * 2;
				case ImageFormat::RG32F:		return 4 * 2;
				case ImageFormat::RGB:			return 3;
				case ImageFormat::RGBA:			return 4;
				case ImageFormat::RGBA16F:		return 2 * 4;
				case ImageFormat::RGBA32F:		return 4 * 4;
				case ImageFormat::B10R11G11UF:	return 4;
			}

			PG_ASSERT(false, "");
			return -1;
		}

		inline constexpr bool IsIntegerBased(const ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::R8UI:
				case ImageFormat::R16UI:
				case ImageFormat::R32UI:
				case ImageFormat::DEPTH32FSTENCIL8UINT:
					return true;
				case ImageFormat::R8UN:
				case ImageFormat::R32F:
				case ImageFormat::RG8:
				case ImageFormat::RG16F:
				case ImageFormat::RG32F:
				case ImageFormat::RGB:
				case ImageFormat::RGBA:
				case ImageFormat::RGBA16F:
				case ImageFormat::RGBA32F:
				case ImageFormat::B10R11G11UF:
				case ImageFormat::DEPTH32F:
				case ImageFormat::DEPTH24STENCIL8:
					return false;
			}

			PG_ASSERT(false, "");
			return false;
		}

		inline uint32_t CalculateMipCount(uint32_t width, uint32_t height)
		{
			return static_cast<uint32_t>(glm::floor(glm::log2(glm::min<uint32_t>(width, height)))) + 1;
		}

		inline constexpr uint32_t GetImageMemorySize(ImageFormat format, uint32_t width, uint32_t height)
		{
			return width * height * GetImageFormatBPP(format);
		}

		inline constexpr bool IsDepthFormat(ImageFormat format)
		{
			if (format == ImageFormat::DEPTH24STENCIL8 || format == ImageFormat::DEPTH32F || format == ImageFormat::DEPTH32FSTENCIL8UINT)
				return true;

			return false;
		}

		inline constexpr VkFormat GetVulkanImageFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::R8UN:					return VK_FORMAT_R8_UNORM;
				case ImageFormat::R8UI:					return VK_FORMAT_R8_UINT;
				case ImageFormat::R16UI:                return VK_FORMAT_R16_UINT;
				case ImageFormat::R32UI:                return VK_FORMAT_R32_UINT;
				case ImageFormat::R32F:					return VK_FORMAT_R32_SFLOAT;
				case ImageFormat::RG8:				    return VK_FORMAT_R8G8_UNORM;
				case ImageFormat::RG16F:				return VK_FORMAT_R16G16_SFLOAT;
				case ImageFormat::RG32F:				return VK_FORMAT_R32G32_SFLOAT;
				case ImageFormat::RGB:					return VK_FORMAT_R8G8B8_UNORM;
				case ImageFormat::RGBA:					return VK_FORMAT_R8G8B8A8_UNORM;
				case ImageFormat::RGBA16F:				return VK_FORMAT_R16G16B16A16_SFLOAT;
				case ImageFormat::RGBA32F:				return VK_FORMAT_R32G32B32A32_SFLOAT;
				case ImageFormat::B10R11G11UF:			return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
				case ImageFormat::DEPTH32FSTENCIL8UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
				case ImageFormat::DEPTH32F:				return VK_FORMAT_D32_SFLOAT;
				case ImageFormat::DEPTH24STENCIL8:		return RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetDepthFormat();
			}

			PG_ASSERT(false, "");
			return VK_FORMAT_UNDEFINED;
		}
	}

}