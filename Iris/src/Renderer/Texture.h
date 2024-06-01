#pragma once

#include "AssetManager/Asset/Asset.h"
#include "Core/Base.h"
#include "Core/Buffer.h"
#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/VulkanAllocator.h"

#include <glm/glm.hpp>
#include <glm/gtc/integer.hpp>
#include <vulkan/vulkan.h>

namespace Iris {

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

		SRGB,
		SRGBA,

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
		// Set by User and Also set internally if needed, ignored in case of storage images
		ImageUsage Usage = ImageUsage::Texture;

		// Set by user
		bool CreateSampler = true;
		// Set by user
		TextureWrap WrapMode = TextureWrap::Repeat;
		// Set by user
		TextureFilter FilterMode = TextureFilter::Linear;

		// Multisampled Image... (1, 2, 4, 8, 16, 32, 64)
		uint32_t Samples = 1;

		// Used for Transfer operations? (Affects the usage of the image)
		bool Trasnfer = false;

		// Set internally! true if the usage is texture, false if usage is attachment
		bool GenerateMips = false;
		// DO NOT SET THIS. This will be determined up on invalidation and is there for debugging purposes.
		uint32_t Mips = 0;

		// TODO: We could store a cache map for per-layer image views and another for per-mip image views and to create them we just loop
		// TODO: on the mipCount and the loop on Layers and just change the subResourceRange for VkImageViewCreateInfo
		// Set by user. Texture might an array useful for shadow mapping
		uint32_t Layers = 1;
	};

	class Texture2D : public Asset
	{
	public:
		// NOTE: If the first constructor is used then an Invalidate call should follow to get a valid image
		Texture2D(const TextureSpecification& spec);
		Texture2D(const TextureSpecification& spec, const std::string& filePath);
		Texture2D(const TextureSpecification& spec, Buffer imageData);
		~Texture2D();

		// NOTE: If CreateNull is used then an Invalidate call should follow to get a valid image
		[[nodiscard]] static Ref<Texture2D> CreateNull(const TextureSpecification& spec);
		[[nodiscard]] static Ref<Texture2D> Create(const TextureSpecification& spec, const std::filesystem::path& filePath);
		[[nodiscard]] static Ref<Texture2D> Create(const TextureSpecification& spec, const std::string& filePath);
		[[nodiscard]] static Ref<Texture2D> Create(const TextureSpecification& spec, Buffer imageData = Buffer());

		void Invalidate();
		void Resize(uint32_t width, uint32_t height);
		void GenerateMips();
		void Release();

		uint64_t GetHash() const { return reinterpret_cast<uint64_t>(m_ImageView); }

		void CopyToHostBuffer(Buffer& buffer, bool writeMips = false) const;

		const std::string& GetAssetPath() const noexcept { return m_AssetPath; }
		ImageFormat GetFormat() const noexcept { return m_Specification.Format; }
		uint32_t GetWidth() const noexcept { return m_Specification.Width; }
		uint32_t GetHeight() const noexcept { return m_Specification.Height; }
		glm::vec2 GetSize() const noexcept { return { m_Specification.Width, m_Specification.Height }; }
		bool Loaded() const { return m_ImageView != nullptr; }

		const VkImage GetVulkanImage() const { return m_Image; }
		const VkImageView GetVulkanImageView() const { return m_ImageView; }
		const VkSampler GetVulkanSampler() const { return m_Sampler; }
		const VmaAllocation GetMemoryAllocation() const { return m_MemoryAllocation; }

		const VkDescriptorImageInfo& GetDescriptorImageInfo() const { return m_DescriptorInfo; }

		TextureSpecification& GetTextureSpecification() { return m_Specification; }
		const TextureSpecification& GetTextureSpecification() const { return m_Specification; }

		uint32_t GetMipLevelCount() const;
		glm::ivec2 GetMipSize(uint32_t mip) const;

		static AssetType GetStaticType() { return AssetType::Texture; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

	private:
		std::string m_AssetPath;
		TextureSpecification m_Specification;

		// Image info
		VkImage m_Image = nullptr;
		VkImageView m_ImageView = nullptr;
		VkSampler m_Sampler = nullptr;
		VmaAllocation m_MemoryAllocation = nullptr;

		Buffer m_ImageData; // Local storage of the image

		VkDescriptorImageInfo m_DescriptorInfo = {};
	};

	class TextureCube : public Asset
	{
	public:
		TextureCube(const TextureSpecification& spec, Buffer data = Buffer());
		~TextureCube();

		[[nodiscard]] inline static Ref<TextureCube> Create(const TextureSpecification& spec, Buffer data = Buffer())
		{
			return CreateRef<TextureCube>(spec, data);
		}

		void Invalidate();
		void GenerateMips(bool readonly = false);
		void Release();
		VkImageView CreateImageViewSingleMip(uint32_t mip);

		uint64_t GetHash() const { return reinterpret_cast<uint64_t>(m_ImageView); }

		void CopyToHostBuffer(Buffer& buffer, bool writeMips = false) const;
		void CopyFromBufer(const Buffer& buffer, uint32_t mips);

		const std::string& GetAssetPath() const noexcept { return m_AssetPath; }
		ImageFormat GetFormat() const { return m_Specification.Format; }
		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }
		glm::vec2 GetSize() const noexcept { return { m_Specification.Width, m_Specification.Height }; }
		bool Loaded() const { return m_ImageView != nullptr; }

		const VkImage GetVulkanImage() const { return m_Image; }
		const VkImageView GetVulkanImageView() const { return m_ImageView; }
		const VkSampler GetVulkanSampler() const { return m_Sampler; }
		const VmaAllocation GetMemoryAllocation() const { return m_MemoryAllocation; }

		const VkDescriptorImageInfo& GetDescriptorImageInfo() const { return m_DescriptorInfo; }

		TextureSpecification& GetTextureSpecification() { return m_Specification; }
		const TextureSpecification& GetTextureSpecification() const { return m_Specification; }

		uint32_t GetMipLevelCount() const;
		glm::ivec2 GetMipSize(uint32_t mip) const;

		static AssetType GetStaticType() { return AssetType::EnvironmentMap; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

	private:
		std::string m_AssetPath; // TODO: Is this gonna be here? since the 
		TextureSpecification m_Specification;

		// Image ifno
		VkImage m_Image = nullptr;
		VkImageView m_ImageView = nullptr;
		VkSampler m_Sampler = nullptr;
		VmaAllocation m_MemoryAllocation = nullptr;

		Buffer m_ImageData; // Local storage of the image

		bool m_MipsGenerated = false;

		VkDescriptorImageInfo m_DescriptorInfo = {};

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
				case ImageFormat::SRGB:			return 3;
				case ImageFormat::SRGBA:		return 4;
				case ImageFormat::DEPTH32F:		return 4;
			}

			IR_ASSERT(false);
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
				case ImageFormat::SRGB:
				case ImageFormat::SRGBA:
				case ImageFormat::DEPTH32F:
				case ImageFormat::DEPTH24STENCIL8:
					return false;
			}

			IR_ASSERT(false);
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
				case ImageFormat::SRGB:					return VK_FORMAT_R8G8B8_SRGB;
				case ImageFormat::SRGBA:				return VK_FORMAT_R8G8B8A8_SRGB;
				case ImageFormat::DEPTH32FSTENCIL8UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
				case ImageFormat::DEPTH32F:				return VK_FORMAT_D32_SFLOAT;
				case ImageFormat::DEPTH24STENCIL8:		return RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetDepthFormat();
			}

			IR_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}

		inline constexpr VkSampleCountFlagBits GetSamplerCount(uint32_t samples)
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

			IR_ASSERT(false);
			return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
		}
	}

}