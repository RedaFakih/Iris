#pragma once

#include "Base.h"
#include "Device.h"

#include <vulkan/vulkan.h>

namespace vkPlayground::Renderer {

	class RendererContext
	{
	public:
		RendererContext();
		~RendererContext();

		static Scope<RendererContext> Create();

		void Init();

	private:
		Ref<VulkanDevice> m_Device;
		Ref<VulkanPhysicalDevice> m_PhysicalDevice;

		static VkInstance s_VulkanInstance;

		VkDebugUtilsMessengerEXT m_DebugUtilsMessenger = VK_NULL_HANDLE;

	};

}