#include "SwapChain.h"

#include "Vulkan.h"

#include <glfw/glfw3.h>

namespace vkPlayground::Renderer {

	void SwapChain::Init(VkInstance instance, const Ref<VulkanDevice>& vulkanDevice)
	{
		m_Instance = instance;
		m_Device = vulkanDevice;
	}

	void SwapChain::InitSurface(GLFWwindow* windowHandle)
	{
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();

		VK_CHECK_RESULT(glfwCreateWindowSurface(m_Instance, windowHandle, nullptr, &m_Surface));

		// Get available queue family properties...
		// Querying for presentation support...
		// Better is to find a queue that supports both graphics and presentation.
		uint32_t queueCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);
		PG_ASSERT(queueCount >= 1, "");

		std::vector<VkQueueFamilyProperties> queueProps(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());

		// Iterate over each queue to learn whether it supports presenting:
		// Find a queue with present support
		// Will be used to present the swap chain images to the windowing system
		std::vector<VkBool32> supportPresent(queueCount);
		for (uint32_t i = 0; i < queueCount; i++)
		{
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_Surface, &supportPresent[i]);
		}

		// Search for a graphics and a present queue in the array of queue
		// families, try to find one that supports both
		uint32_t graphicsQueueNodeIndex = UINT32_MAX;
		uint32_t presentQueueNodeIndex = UINT32_MAX;
		for (uint32_t i = 0; i < queueCount; i++)
		{
			if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
			{
				if (graphicsQueueNodeIndex == UINT32_MAX)
				{
					graphicsQueueNodeIndex = i;
				}

				if (supportPresent[i] == VK_TRUE)
				{
					graphicsQueueNodeIndex = i;
					presentQueueNodeIndex = i;
					break;
				}
			}
		}

		if (presentQueueNodeIndex == UINT32_MAX)
		{
			// If there's no queue that supports both present and graphics
			// try to find a separate present queue
			for (uint32_t i = 0; i < queueCount; i++)
			{
				if (supportPresent[i] == VK_TRUE)
				{
					presentQueueNodeIndex = i;
					break;
				}
			}
		}

		// At this point we should have definetly found a queue that supports presenting to it
		PG_ASSERT(graphicsQueueNodeIndex != UINT32_MAX, "");
		PG_ASSERT(presentQueueNodeIndex != UINT32_MAX, "");

		m_QueueNodeIndex = graphicsQueueNodeIndex;

		// TODO: Retrieve the Present Queue????????????

		FindImageFormatAndColorSpace();
	}

	void SwapChain::Create(uint32_t* width, uint32_t* height, bool vsync)
	{
		m_VSync = vsync;

		VkDevice device = m_Device->GetVulkanDevice();
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();

		VkSwapchainKHR oldSwapChain = m_SwapChain;

		// Get physical device surface properties and formats
		VkSurfaceCapabilitiesKHR surfaceCaps;
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &surfaceCaps));

		// Get available present modes...
		uint32_t presentModeCount;
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, nullptr));
		PG_ASSERT(presentModeCount > 0, "");
		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, presentModes.data()));

		// Select a present mode for the swapchain

		// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
		// This mode waits for the vertical blank ("v-sync")
		VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

		// If v-sync is not requested, try to find a mailbox mode
		// It's the lowest latency non-tearing present mode available
		if (!vsync)
		{
			for (size_t i = 0; i < presentModeCount; i++)
			{
				if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
					break;
				}

				if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
				{
					swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
				}
			}
		}

		VkExtent2D swapchainExtent = {};
		// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
		if (surfaceCaps.currentExtent.width == (uint32_t)-1)
		{
			// If the surface size is undefined, the size is set to
			// the size of the images requested.
			swapchainExtent.width = *width;
			swapchainExtent.height = *height;
		}
		else
		{
			// If the surface size is defined, the swap chain size must match
			swapchainExtent = surfaceCaps.currentExtent;
			*width = surfaceCaps.currentExtent.width;
			*height = surfaceCaps.currentExtent.height;
		}

		m_Width = *width;
		m_Height = *height;

		// Determine the number of images...
		uint32_t numberOfSwapChainImages = surfaceCaps.minImageCount + 1;
		if ((surfaceCaps.maxImageCount > 0) && (numberOfSwapChainImages > surfaceCaps.maxImageCount))
		{
			numberOfSwapChainImages = surfaceCaps.maxImageCount; // Basically clamp it to the maximum allowed number of images
		}

		// Find the transformation of the surface...
		VkSurfaceTransformFlagsKHR preTransform;
		if (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		{
			// We prefer a non rotated transform
			preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else
		{
			preTransform = surfaceCaps.currentTransform;
		}

		// Find a supported composite alpha format (not all devices support alpha opaque)...
		VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		// Simply select the first composite alpha format available..
		std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags =
		{
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
		};

		for (VkCompositeAlphaFlagBitsKHR& compositeAlphaFlag : compositeAlphaFlags)
		{
			if (surfaceCaps.supportedCompositeAlpha & compositeAlphaFlag)
			{
				compositeAlpha = compositeAlphaFlag;
				break;
			}
		}

		VkSwapchainCreateInfoKHR swapchainCI = {};
		swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCI.pNext = nullptr;
		swapchainCI.surface = m_Surface;
		swapchainCI.minImageCount = numberOfSwapChainImages;
		swapchainCI.imageFormat = m_ColorFormat;
		swapchainCI.imageColorSpace = m_ColorSpace;
		swapchainCI.imageExtent = swapchainExtent;
		swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
		swapchainCI.imageArrayLayers = 1;
		swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCI.queueFamilyIndexCount = 0;
		swapchainCI.pQueueFamilyIndices = nullptr;
		swapchainCI.presentMode = swapchainPresentMode;
		swapchainCI.oldSwapchain = oldSwapChain;
		// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
		swapchainCI.clipped = VK_TRUE;
		swapchainCI.compositeAlpha = compositeAlpha;

		// Enable transfer source on swap chain images if supported
		if (surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		{
			swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}

		// Enable transfer destination on swap chain images if supported
		if (surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		{
			swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		VK_CHECK_RESULT(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &m_SwapChain));

		if (oldSwapChain)
		{
			vkDestroySwapchainKHR(device, oldSwapChain, nullptr);
		}

		for (auto& image : m_Images)
			vkDestroyImageView(device, image.ImageView, nullptr);
		m_Images.clear();

		// Get the swap chain images
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device, m_SwapChain, &m_ImageCount, nullptr));
		PG_ASSERT(m_ImageCount, "");
		m_Images.resize(m_ImageCount);
		m_VulkanImages.resize(m_ImageCount);
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device, m_SwapChain, &m_ImageCount, m_VulkanImages.data()));

		// Get the swap chain buffers containing the image and imageview
		for (uint32_t i = 0; i < m_ImageCount; i++)
		{
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.pNext = nullptr;
			createInfo.format = m_ColorFormat;
			createInfo.image = m_VulkanImages[0];
			createInfo.components = {
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_A
			};
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.flags = 0;

			m_Images[i].Image = m_VulkanImages[i];

			VK_CHECK_RESULT(vkCreateImageView(device, &createInfo, nullptr, &m_Images[i].ImageView));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, "SwapChain ImageView: " + std::to_string(i), m_Images[i].ImageView);
		}

		// TODO: Command buffers?
		// TODO: Synchronization Objects?
		// TODO: RenderPasses?
		// TODO: Per Image Framebuffers?
	}
	
	void SwapChain::Destroy()
	{
		VkDevice device = m_Device->GetVulkanDevice();

		vkDeviceWaitIdle(device);

		if (m_SwapChain)
			vkDestroySwapchainKHR(device, m_SwapChain, nullptr);

		for (auto& image : m_Images)
			vkDestroyImageView(device, image.ImageView, nullptr);

		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
	
		vkDeviceWaitIdle(device);
	}

	void SwapChain::BeginFrame()
	{

	}

	void SwapChain::Present()
	{

	}

	void SwapChain::FindImageFormatAndColorSpace()
	{
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();

		// Get the list of supported surface formats...
		uint32_t formatCount;
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr));
		PG_ASSERT(formatCount > 0, "");
		std::vector<VkSurfaceFormatKHR> formats(formatCount);
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, formats.data()));

		// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
		// there is no preferered format, so we assume VK_FORMAT_B8G8R8A8_UNORM
		if ((formatCount == 1) && (formats[0].format == VK_FORMAT_UNDEFINED))
		{
			m_ColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
			m_ColorSpace = formats[0].colorSpace;
		}
		else
		{
			// iterate over the list of available surface format and
			// check for the presence of VK_FORMAT_B8G8R8A8_UNORM
			bool found_B8G8R8A8_UNORM = false;
			for (VkSurfaceFormatKHR& surfaceFormat : formats)
			{
				if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
				{
					m_ColorFormat = surfaceFormat.format;
					m_ColorSpace = surfaceFormat.colorSpace;
					found_B8G8R8A8_UNORM = true;
					break;
				}
			}

			// in case VK_FORMAT_B8G8R8A8_UNORM is not available
			// select the first available color format
			if (!found_B8G8R8A8_UNORM)
			{
				m_ColorFormat = formats[0].format;
				m_ColorSpace = formats[0].colorSpace;
			}
		}
	}

}