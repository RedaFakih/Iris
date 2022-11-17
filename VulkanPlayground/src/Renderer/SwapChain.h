#pragma once

#include "Core/Base.h"
#include "Device.h"

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace vkPlayground::Renderer {

	class SwapChain
	{
	public:
		SwapChain() = default;
		~SwapChain() = default;

		void Init(VkInstance instance, const Ref<VulkanDevice>& vulkanDevice);
		void InitSurface(GLFWwindow* windowHandle);
		void Create(uint32_t* width, uint32_t* height, bool vsync);
		void Destroy();

		void OnResize(uint32_t width, uint32_t height) { /* TODO: Implement Resizing...*/width = 0; height = 0; }

		void BeginFrame();
		void Present();		

	private:
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

		uint32_t m_QueueNodeIndex = UINT32_MAX;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		VkSurfaceKHR m_Surface; // https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Window_surface

		friend class RenderContext;

	};

}