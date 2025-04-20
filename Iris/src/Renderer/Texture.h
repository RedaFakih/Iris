#pragma once

#include "AssetManager/Asset/Asset.h"
#include "Core/Base.h"
#include "Core/Buffer.h"
#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/VulkanAllocator.h"

#include <glm/glm.hpp>
#include <glm/gtc/integer.hpp>
#include <vulkan/vulkan.h>

#include <filesystem>

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

		Depth32F,
		Depth32FStencil8UINT,
		Depth24UNORMStencil8UINT,
		DepthDefault // Default device depth format
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
		Storage
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
		// Set by User and Also set internally if needed. IGNORED by StorageImages
		ImageUsage Usage = ImageUsage::Texture;

		// Set by user. IGNORED by StorageImages
		bool CreateSampler = true;
		// Set by user
		TextureWrap WrapMode = TextureWrap::Repeat;
		// Set by user
		TextureFilter FilterMode = TextureFilter::Linear;

		// Multisampled Image... (1, 2, 4, 8, 16, 32, 64). NOTE: NOT SUPPORTED, DO NOT SET
		uint32_t Samples = 1;

		// Used for Transfer operations? (Affects the usage of the image, usually leave as true)
		bool Transfer = true;

		// If usage is Attachment then it will be overriden to false otherwise used value is one set by user.
		bool GenerateMips = true;
		// DO NOT SET THIS. This will be determined up on invalidation and is there for debugging purposes.
		uint32_t Mips = 0;

		// You could have a mipped, layered image. However if ImageUsage::Attachment is set then you are ONLY allowed to have a layered image, GenerateMips HAS to be false.
		// IGNORED by StorageImages
		uint32_t Layers = 1;
	};

	/*
	 * A bit of a change in the Texture and TextureCube API is that now we provide the option to pass in a command buffer to execute the commands on
	 * If no command buffer is passed in then all the commands are executed on the Graphics queue, and if THERE WAS a command buffer passed in then the commands
	 * will be ASSUMED to be running on the compute queue since that is basically the only other queue that can run almost all commands
	 */

	class ImageView;

	class Texture2D : public Asset
	{
	public:
		// NOTE: If the first constructor is used then an Invalidate call should follow to get a valid image
		explicit Texture2D(const TextureSpecification& spec);
		explicit Texture2D(const TextureSpecification& spec, const std::string& filePath, VkCommandBuffer commandBuffer = nullptr);
		explicit Texture2D(const TextureSpecification& spec, Buffer imageData, VkCommandBuffer commandBuffer = nullptr);
		~Texture2D();

		// NOTE: If CreateNull is used then an Invalidate call should follow to get a valid image
		[[nodiscard]] static Ref<Texture2D> CreateNull(const TextureSpecification& spec)
		{
			return CreateRef<Texture2D>(spec);
		}

		[[nodiscard]] static Ref<Texture2D> Create(const TextureSpecification& spec, const std::filesystem::path& filePath, VkCommandBuffer commandBuffer = nullptr)
		{
			return Texture2D::Create(spec, filePath.string(), commandBuffer);
		}

		[[nodiscard]] static Ref<Texture2D> Create(const TextureSpecification& spec, const std::string& filePath, VkCommandBuffer commandBuffer = nullptr)
		{
			return CreateRef<Texture2D>(spec, filePath, commandBuffer);
		}

		[[nodiscard]] static Ref<Texture2D> Create(const TextureSpecification& spec, Buffer imageData = Buffer(), VkCommandBuffer commandBuffer = nullptr)
		{
			return CreateRef<Texture2D>(spec, imageData, commandBuffer);
		}

		void Invalidate(VkCommandBuffer commandBuffer = nullptr);
		void Resize(uint32_t width, uint32_t height, VkCommandBuffer commandBuffer = nullptr);
		void GenerateMips(VkCommandBuffer commandBuffer = nullptr);
		void Release();

		Ref<ImageView> CreateImageViewSingleMip(uint32_t mip);
		Ref<ImageView> CreateImageViewSingleLayer(uint32_t layer);
		std::vector<Ref<ImageView>> CreatePerLayerImageViews();

		uint64_t GetHash() const { return reinterpret_cast<uint64_t>(m_ImageView); }

		void CopyToHostBuffer(Buffer& buffer, bool writeMips = false, VkCommandBuffer commandBuffer = nullptr) const;

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
		uint32_t GetNumLayers() const { return m_Specification.Layers; }
		glm::ivec2 GetMipSize(uint32_t mip) const;
		uint32_t GetClosestMipLevel(uint32_t width, uint32_t height) const;

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
		explicit TextureCube(const TextureSpecification& spec, Buffer data = Buffer(), VkCommandBuffer commandBuffer = nullptr);
		~TextureCube();

		[[nodiscard]] inline static Ref<TextureCube> Create(const TextureSpecification& spec, Buffer data = Buffer(), VkCommandBuffer commandBuffer = nullptr)
		{
			return CreateRef<TextureCube>(spec, data, commandBuffer);
		}

		void Invalidate(VkCommandBuffer commandBuffer = nullptr);
		void GenerateMips(bool readonly = false, VkCommandBuffer commandBuffer = nullptr);
		void Release();
		
		Ref<ImageView> CreateImageViewSingleMip(uint32_t mip);

		uint64_t GetHash() const { return reinterpret_cast<uint64_t>(m_ImageView); }

		// Always writes mips if any
		void CopyToHostBuffer(Buffer& buffer, VkCommandBuffer commandBuffer = nullptr) const;
		void CopyFromHostBufer(const Buffer& buffer, uint32_t mips, VkCommandBuffer commandBuffer = nullptr);

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
		uint32_t GetNumLayers() const { return m_Specification.Layers; }
		glm::ivec2 GetMipSize(uint32_t mip) const;
		uint32_t GetClosestMipLevel(uint32_t width, uint32_t height) const;

		static AssetType GetStaticType() { return AssetType::EnvironmentMap; }
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

		bool m_MipsGenerated = false;

		VkDescriptorImageInfo m_DescriptorInfo = {};

	};

	struct ImageViewSpecification
	{
		std::string DebugName;

		// IMPORTANT: Only set ONE of these, DO NOT SET A COMBINATION OF THEM. Depending on what the user sets the image view type and other info will be determined / deduced
		Ref<Texture2D> Image = nullptr;
		Ref<TextureCube> CubeImage = nullptr;

		// Desired mip for the image view
		// Leave at - 1 if you want to include ALL mips of the image in the image view
		int Mip = -1;
		// Desired layer for the image view
		// Leave at -1 if you want to include ALL layers of the image in the image view
		int Layer = -1;
	};


	class ImageView : public RefCountedObject
	{
	public:
		explicit ImageView(const ImageViewSpecification& spec, bool deferInvalidation = true);
		virtual ~ImageView();

		[[nodiscard]] static Ref<ImageView> Create(const ImageViewSpecification& spec, bool deferInvalidation = true)
		{
			return CreateRef<ImageView>(spec, deferInvalidation);
		}

		void Invalidate();
		void RT_Invalidate();

		VkImageView GetVulkanImageView() const { return m_ImageView; }

		const VkDescriptorImageInfo& GetDescriptorImageInfo() const { return m_DescriptorInfo; }

	private:
		ImageViewSpecification m_Specification;

		VkImageView m_ImageView = nullptr;

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
			}

			IR_ASSERT(false);
			return UINT32_MAX;
		}

		inline constexpr bool IsIntegerBased(const ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::R8UI:
				case ImageFormat::R16UI:
				case ImageFormat::R32UI:
				case ImageFormat::Depth24UNORMStencil8UINT:
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
				case ImageFormat::Depth32F:
				case ImageFormat::Depth32FStencil8UINT:
				case ImageFormat::DepthDefault: // We do not know but most probably no
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
			if (format == ImageFormat::Depth24UNORMStencil8UINT || format == ImageFormat::Depth32FStencil8UINT || format == ImageFormat::Depth32F || format == ImageFormat::DepthDefault)
				return true;

			return false;
		}

		inline constexpr bool IsDepthOnly(ImageFormat format)
		{
			if (format == ImageFormat::Depth32F)
				return true;

			return false;
		}

		inline constexpr VkFormat GetVulkanImageFormat(ImageFormat format)
		{
			switch (format)
			{
				// Color Formats
				case ImageFormat::R8UN:							return VK_FORMAT_R8_UNORM;
				case ImageFormat::R8UI:							return VK_FORMAT_R8_UINT;
				case ImageFormat::R16UI:						return VK_FORMAT_R16_UINT;
				case ImageFormat::R32UI:						return VK_FORMAT_R32_UINT;
				case ImageFormat::R32F:							return VK_FORMAT_R32_SFLOAT;
				case ImageFormat::RG8:							return VK_FORMAT_R8G8_UNORM;
				case ImageFormat::RG16F:						return VK_FORMAT_R16G16_SFLOAT;
				case ImageFormat::RG32F:						return VK_FORMAT_R32G32_SFLOAT;
				case ImageFormat::RGB:							return VK_FORMAT_R8G8B8_UNORM;
				case ImageFormat::RGBA:							return VK_FORMAT_R8G8B8A8_UNORM;
				case ImageFormat::RGBA16F:						return VK_FORMAT_R16G16B16A16_SFLOAT;
				case ImageFormat::RGBA32F:						return VK_FORMAT_R32G32B32A32_SFLOAT;
				case ImageFormat::B10R11G11UF:					return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
				case ImageFormat::SRGB:							return VK_FORMAT_R8G8B8_SRGB;
				case ImageFormat::SRGBA:						return VK_FORMAT_R8G8B8A8_SRGB;
				// Depth Formats
				case ImageFormat::Depth32F:						return VK_FORMAT_D32_SFLOAT;
				case ImageFormat::Depth32FStencil8UINT:			return VK_FORMAT_D32_SFLOAT_S8_UINT;
				case ImageFormat::Depth24UNORMStencil8UINT:		return VK_FORMAT_D24_UNORM_S8_UINT;
				case ImageFormat::DepthDefault:					return RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetDepthFormat();
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