#pragma once

#include "Base.h"
#include "Device.h"

#include <vulkan/vulkan.h>

#define PG_USE_DEBUG_REPORT_CALLBACK_EXT 0

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

#if PG_USE_DEBUG_REPORT_CALLBACK_EXT
		VkDebugReportCallbackEXT m_DebugReportCallback = VK_NULL_HANDLE;
#endif
		VkDebugUtilsMessengerEXT m_DebugUtilsMessenger = VK_NULL_HANDLE;

	};

}