#include "vkPch.h"
#include "SwapChain.h"

#include "Renderer/Core/Vulkan.h"
#include "Renderer/Renderer.h"

#include <glfw/glfw3.h>

namespace vkPlayground {

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
		// If width and height are equal to the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
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

		if (*width == 0 || *height == 0)
			return;

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
		std::array<VkCompositeAlphaFlagBitsKHR, 4> compositeAlphaFlags =
		{
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
		};

		for (VkCompositeAlphaFlagBitsKHR compositeAlphaFlag : compositeAlphaFlags)
		{
			if (surfaceCaps.supportedCompositeAlpha & compositeAlphaFlag)
			{
				compositeAlpha = compositeAlphaFlag;
				break;
			}
		}

		VkSwapchainCreateInfoKHR swapchainCI = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = nullptr,
			.surface = m_Surface,
			.minImageCount = numberOfSwapChainImages,
			.imageFormat = m_ColorFormat,
			.imageColorSpace = m_ColorSpace,
			.imageExtent = swapchainExtent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
			.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform,
			.compositeAlpha = compositeAlpha,
			.presentMode = swapchainPresentMode,
			// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
			.clipped = VK_TRUE,
			.oldSwapchain = oldSwapChain
		};

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
			vkDestroySwapchainKHR(device, oldSwapChain, nullptr);

		// NOTE: This here is to clean the imageViews when we want to resize
		for (auto& image : m_Images)
			vkDestroyImageView(device, image.ImageView, nullptr);
		m_Images.clear();

		// Get the swap chain images
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device, m_SwapChain, &m_ImageCount, nullptr));
		PG_ASSERT(m_ImageCount, "");
		// NOTE: We could here set the Frame in Flight equal to the max number of swapchain images, but some platforms may have > 8 swapchain images which
		// is not really ideal to have 8 frames in flight so we just run whatever the user sets in the ApplicationSpecification::RendererConfig.FramesInFlight
		// Preferebly 2-3 frames in flight NO MORE.
		m_Images.resize(m_ImageCount);
		m_VulkanImages.resize(m_ImageCount);
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device, m_SwapChain, &m_ImageCount, m_VulkanImages.data()));

		// Get the swap chain buffers containing the image and imageview
		for (uint32_t i = 0; i < m_ImageCount; i++)
		{
			VkImageViewCreateInfo createInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.image = m_VulkanImages[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = m_ColorFormat,
				.components = {
					.r = VK_COMPONENT_SWIZZLE_R,
					.g = VK_COMPONENT_SWIZZLE_G,
					.b = VK_COMPONENT_SWIZZLE_B,
					.a = VK_COMPONENT_SWIZZLE_A
				},
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			m_Images[i].Image = m_VulkanImages[i];

			VK_CHECK_RESULT(vkCreateImageView(device, &createInfo, nullptr, &m_Images[i].ImageView));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, "SwapChain ImageView: " + std::to_string(i), m_Images[i].ImageView);
		}

		// Command buffers
		{
			// NOTE: Destroying here in case we are resizing
			for (auto& commandBuffer : m_CommandBuffers)
				vkDestroyCommandPool(device, commandBuffer.CommandPool, nullptr);

			VkCommandPoolCreateInfo commandPoolInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
				.queueFamilyIndex = m_QueueNodeIndex
			};

			VkCommandBufferAllocateInfo cbAllocateInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};

			m_CommandBuffers.resize(m_ImageCount);
			for (auto& commandBuffer : m_CommandBuffers)
			{
				VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandBuffer.CommandPool));

				cbAllocateInfo.commandPool = commandBuffer.CommandPool;
				VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cbAllocateInfo, &commandBuffer.CommandBuffer));
			}
		}

		// Synchronization Objects
		{
			/*
			 * IMPORTANT:
			 *	- NOTE: Semaphores: Are for synchronization between GPU-GPU tasks and NOT GPU-CPU tasks.
			 *  - NOTE: Fences: Are for GPU-to-CPU synchronization. (When the CPU wants to know if the GPU finished we use a fence)
			 * The events that we should be worried of ordering and synchronizing `basically` is the following:
			 *	- Acquire an image from the swapchain
			 *  - Execute commands that draw on the acquired image
			 *	- Present the image on the screen, returning it to the swapchain
			 */
		
			// These do not need to be created and destroyed each time we resize so we put behind a gaurd as long as the semaphores are valid
			if (!m_Semaphores.RenderComplete || !m_Semaphores.PresentComplete)
			{
				VkSemaphoreCreateInfo semaphoreInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
				VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_Semaphores.RenderComplete));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, "Swapchain Semaphore RenderComplete", m_Semaphores.RenderComplete);
				VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_Semaphores.PresentComplete));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, "Swapchain Semaphore PresentComplete", m_Semaphores.PresentComplete);
			}

			if (m_WaitFences.size() != m_ImageCount)
			{
				VkFenceCreateInfo fenceInfo = { 
					.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, 
					.flags = VK_FENCE_CREATE_SIGNALED_BIT // Creates the fence in signaled state already
				};

				m_WaitFences.resize(m_ImageCount);
				for (auto& fence : m_WaitFences)
				{
					VK_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &fence));
					VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_FENCE, "Swapchain Fence", fence);
				}
			}
		}

		// RenderPass
		{
			// NOTE: We dont need to recreate the RenderPass every time we resize so we can put behind a gaurd as long as the renderpass is valid
			if (!m_RenderPass)
			{
				VkAttachmentDescription colorAttachment = {
					.format = m_ColorFormat,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
				};

				VkAttachmentReference colorAttachmentsRef = {
					.attachment = 0,
					.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				};

				VkSubpassDescription subpassDesc = {
					.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
					.inputAttachmentCount = 0,
					.pInputAttachments = nullptr,
					.colorAttachmentCount = 1,
					.pColorAttachments = &colorAttachmentsRef,
					.pResolveAttachments = nullptr,
					.pDepthStencilAttachment = nullptr,
					.preserveAttachmentCount = 0,
					.pPreserveAttachments = nullptr
				};

				// NOTE: Specify memory and execution dependencies between subpasses!
				// There are 2 built-in dependencies that take care of transitions at the start and end of the renderpass
				// but the former (start) does not occur at the right time. It assumes that the transition occurs at the start of the pipeline
				// but at that point we have not acquired the image yet!
				VkSubpassDependency subPassDependency = {
					.srcSubpass = VK_SUBPASS_EXTERNAL, // Refers to implicit subpass
					.dstSubpass = 0, // Refers to our subpass (it is the first and only one).
					// We need to wait for the swapchain to finish reading from the image before we can access it
					.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					// Operation that should wait are in the color attachment stage and involve writing to it
					.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
				};

				VkRenderPassCreateInfo renderPassInfo = {
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
					.attachmentCount = 1,
					.pAttachments = &colorAttachment,
					.subpassCount = 1,
					.pSubpasses = &subpassDesc,
					.dependencyCount = 1,
					.pDependencies = &subPassDependency
				};

				VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_RENDER_PASS, "Swapchain RenderPass", m_RenderPass);
			}
		}

		// Create Framebuffers for every swapchain image
		{
			// NOTE: This here is to handle the case when we are resizing
			for (VkFramebuffer& framebuffer : m_Framebuffers)
				vkDestroyFramebuffer(device, framebuffer, nullptr);

			VkFramebufferCreateInfo fbInfo = {
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass = m_RenderPass,
				.attachmentCount = 1,
				.width = m_Width,
				.height = m_Height,
				.layers = 1
			};

			m_Framebuffers.resize(m_ImageCount);
			for (uint32_t i = 0; i < m_Framebuffers.size(); i++)
			{
				fbInfo.pAttachments = &m_Images[i].ImageView;
				VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbInfo, nullptr, &m_Framebuffers[i]));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_FRAMEBUFFER, fmt::format("Swapchain Framebuffer (Frame in flight: {})", i), m_Framebuffers[i]);
			}
		}
	}
	
	void SwapChain::Destroy()
	{
		VkDevice device = m_Device->GetVulkanDevice();

		vkDeviceWaitIdle(device);

		if (m_SwapChain)
			vkDestroySwapchainKHR(device, m_SwapChain, nullptr);

		for (auto& image : m_Images)
			vkDestroyImageView(device, image.ImageView, nullptr);

		for (auto& commandBuffer : m_CommandBuffers)
			vkDestroyCommandPool(device, commandBuffer.CommandPool, nullptr);

		if(m_RenderPass)
			vkDestroyRenderPass(device, m_RenderPass, nullptr);

		for (VkFramebuffer& framebuffer : m_Framebuffers)
			vkDestroyFramebuffer(device, framebuffer, nullptr);

		if (m_Semaphores.RenderComplete)
			vkDestroySemaphore(device, m_Semaphores.RenderComplete, nullptr);

		if (m_Semaphores.PresentComplete)
			vkDestroySemaphore(device, m_Semaphores.PresentComplete, nullptr);

		for (auto& fence : m_WaitFences)
			vkDestroyFence(device, fence, nullptr);

		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
	
		vkDeviceWaitIdle(device);
	}

	void SwapChain::OnResize(uint32_t width, uint32_t height)
	{
		VkDevice device = m_Device->GetVulkanDevice();
		vkDeviceWaitIdle(device);
		Create(&width, &height, m_VSync);
		vkDeviceWaitIdle(device);
	}

	void SwapChain::BeginFrame()
	{
		/*
		 * NOTE: Here before doing anything in this frame we should submit ResourceFree commands of the previous frame since at this point
		 * we know that anything (pipelines, renderpasses...) queued to destruction in the previous frame could now be deleted since there is
		 * nothing that is still using them since that frame is already presented to the swapchain
		 * 
		 * So in some way we create our own render command queue to submit resource free commands to it and at this point `m_CurrentBufferIndex`
		 * has not been updated yet so it is still trailing by a frame so will free everything without problems
		 */
		
		// TODO: Should be done on the Renderthread, so `SwapChain::BeginFrame` should be called inside a `Renderer::Submit`
		RenderCommandQueue& rendererResourceFreeQueue = Renderer::GetRendererResourceReleaseQueue(m_CurrentBufferIndex);
		rendererResourceFreeQueue.Execute();

		m_CurrentImageIndex = AcquireNextImage();

		// We reset the current command pool since it will reset its command buffer
		VK_CHECK_RESULT(vkResetCommandPool(m_Device->GetVulkanDevice(), m_CommandBuffers[m_CurrentBufferIndex].CommandPool, 0));
	}

	void SwapChain::Present()
	{
		VkDevice device = m_Device->GetVulkanDevice();

		// First reset the fence for the frame so that it will be signaled to the CPU when the GPU has finished rendering
		VK_CHECK_RESULT(vkResetFences(device, 1, &m_WaitFences[m_CurrentBufferIndex]));

		// Submission
		// This could be an array and each element corresponds to a semaphore (which will be more than 1)
		VkPipelineStageFlags waitStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1, // NOTE: Specifies the semaphore to wait on before execution begins
			.pWaitSemaphores = &m_Semaphores.PresentComplete,
			.pWaitDstStageMask = &waitStageFlags,
			.commandBufferCount = 1,
			.pCommandBuffers = &m_CommandBuffers[m_CurrentBufferIndex].CommandBuffer,
			.signalSemaphoreCount = 1, // NOTE: Specifies the semaphore to signal once the command buffer has finished execution
			.pSignalSemaphores = &m_Semaphores.RenderComplete
		};

		VK_CHECK_RESULT(vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, m_WaitFences[m_CurrentBufferIndex]));

		// Presentation
		// Present the current commandbuffer to the swapchain
		// Pass the semaphore signaled by commandbuffer submission from the submit info as the wait semaphore for swapchain presentation
		// This ensures that the image is not presented by the windowing system untill all commands have been executed!
		VkResult result;
		{
			VkPresentInfoKHR presentInfo = {
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.pNext = nullptr,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &m_Semaphores.RenderComplete, // We want to wait for the commandbuffer to finish execution
				.swapchainCount = 1,
				.pSwapchains = &m_SwapChain,
				.pImageIndices = &m_CurrentImageIndex,
				.pResults = nullptr // Optional since we only have one swapchain we can use the return value of the present function
			};

			result = vkQueuePresentKHR(m_Device->GetGraphicsQueue(), &presentInfo);
		}

		if (result != VK_SUCCESS)
		{
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			{
				OnResize(m_Width, m_Height); // NOTE: The width and height will be updated from the Event Dispatch
			}
			else
			{
				VK_CHECK_RESULT(result);
			}
		}

		{
			m_CurrentBufferIndex = (m_CurrentBufferIndex + 1) % Renderer::GetConfig().FramesInFlight;

			// Make sure that only one frame is being rendered at a time
			// NOTE: So lets say we have submitted the first N frame in flight to the GPU but the GPU has not finished rendering the First frame we submitted
			// Here we would have to wait for the GPU to finish rendering that image so that we can acquire the swapchain image of that frame to render into
			// again.
			VK_CHECK_RESULT(vkWaitForFences(device, 1, &m_WaitFences[m_CurrentBufferIndex], VK_TRUE, UINT64_MAX));
		}
	}

	uint32_t SwapChain::AcquireNextImage()
	{
		uint32_t imageIndex; // This index will be used to pick the framebuffer we render too (and command buffer we submit commands to).
		// NOTE: If for some reason we fail to acquire the next image according to some problem that is stated by the spec we invalidate
		// the swapchain and try again!
		VkResult result = vkAcquireNextImageKHR(m_Device->GetVulkanDevice(), m_SwapChain, UINT64_MAX, m_Semaphores.PresentComplete, (VkFence)nullptr, &imageIndex);
		if (result != VK_SUCCESS)
		{
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			{
				OnResize(m_Width, m_Height);
				VK_CHECK_RESULT(vkAcquireNextImageKHR(m_Device->GetVulkanDevice(), m_SwapChain, UINT64_MAX, m_Semaphores.PresentComplete, (VkFence)nullptr, &imageIndex));
			}
		}

		return imageIndex;
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

		// NOTE: We are defaulting to UNORM color format
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

		// NOTE: If we want to default to sRGB color format we can do that by trying to find the format and color space listed below
		// Ideally we would at the last composite pass just convert to sRGB in the shader
		// m_ColorFormat = VK_FORMAT_B8G8R8A8_SRGB;
		// m_ColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}

}