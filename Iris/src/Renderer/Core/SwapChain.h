#pragma once

#include "Core/Base.h"
#include "Device.h"
#include "VulkanAllocator.h"

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace Iris {

	class SwapChain
	{
	public:
		enum class ImageLayout : uint8_t
		{
			None = 0,
			Attachment,
			Present
		};

	public:
		SwapChain() = default;
		~SwapChain() = default;

		void Init(VkInstance instance, const Ref<VulkanDevice>& vulkanDevice);
		void InitSurface(GLFWwindow* windowHandle);
		void Create(uint32_t* width, uint32_t* height, bool vsync);
		void Destroy();

		void OnResize(uint32_t width, uint32_t height);

		void BeginFrame();
		void Present();	

		void TransitionImageLayout(SwapChain::ImageLayout layout);

		VkRenderingAttachmentInfo GetCurrentImageInfo() const { return m_SwapChainImagesInfo[m_CurrentImageIndex]; }

		uint32_t GetCurrentBufferIndex() const { return m_CurrentFrameIndex; }
		VkCommandBuffer GetCurrentDrawCommandBuffer() { return GetDrawCommandBuffer(m_CurrentFrameIndex); }
		VkCommandBuffer GetDrawCommandBuffer(uint32_t index) { IR_ASSERT(index < m_CommandBuffers.size(), "Index should be less then amount of commandbuffers"); return m_CommandBuffers[index].CommandBuffer; }

		VkFormat GetColorFormat() const { return m_ColorFormat; }

		inline uint32_t GetImageCount() const { return m_ImageCount; }
		inline uint32_t GetWidth() const { return m_Width; }
		inline uint32_t GetHeight() const { return m_Height; }

		void SetVSync(bool vsync) { m_VSync = vsync; }

	private:
		uint32_t AcquireNextImage();

		void FindImageFormatAndColorSpace();

	private:
		VkInstance m_Instance = nullptr;
		Ref<VulkanDevice> m_Device;
		bool m_VSync = false;

		VkSwapchainKHR m_SwapChain = nullptr;

		VkFormat m_ColorFormat;
		VkColorSpaceKHR m_ColorSpace;

		uint32_t m_ImageCount = 0;
		std::vector<VkImage> m_VulkanImages;

		struct SwapChainImages
		{
			VkImage Image = nullptr;
			VkImageView ImageView = nullptr;
		};
		std::vector<SwapChainImages> m_Images;

		std::vector<VkRenderingAttachmentInfo> m_SwapChainImagesInfo;

		struct SwapchainCommandBuffer
		{
			VkCommandPool CommandPool = nullptr;
			VkCommandBuffer CommandBuffer = nullptr;
		};
		std::vector<SwapchainCommandBuffer> m_CommandBuffers;

		// Swapchain (Signal that an image has been acquired from the swapchain and is ready to be rendered to)
		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		// CommandBuffer (Signal that rendering has finished and the image is ready for presentation)
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;

		// To ensure that only one frame is being presented at a time
		std::vector<VkFence> m_WaitFences;

		uint32_t m_CurrentFrameIndex = 0; // Index of current frame (up to max of frames in flight)
		uint32_t m_CurrentImageIndex = 0; // Index of current swapchain image

		uint32_t m_QueueNodeIndex = UINT32_MAX;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		VkSurfaceKHR m_Surface; // https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Window_surface

		friend class RendererContext;

	};

}