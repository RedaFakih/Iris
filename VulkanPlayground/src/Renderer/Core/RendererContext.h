#pragma once

#include "Core/Base.h"
#include "Renderer/Core/Device.h"

#include <vulkan/vulkan.h>

namespace vkPlayground {

	class RendererContext
	{
	public:
		RendererContext();
		~RendererContext();

		[[nodiscard]] static Ref<RendererContext> Create();

		void Init();

		inline Ref<VulkanDevice> GetDevice() const { return m_Device; }

		inline static VkInstance GetInstance() { return s_VulkanInstance; }

		static Ref<RendererContext> Get();
		inline static Ref<VulkanDevice> GetCurrentDevice() { return Get()->GetDevice(); }

	private:
		Ref<VulkanDevice> m_Device;
		Ref<VulkanPhysicalDevice> m_PhysicalDevice;

		static VkInstance s_VulkanInstance;

		// NOTE: Debug Utils just combine the VK_EXT_debug_report and VK_EXT_debug_marker extensions and has more and better stuff...
		VkDebugUtilsMessengerEXT m_DebugUtilsMessenger = VK_NULL_HANDLE;

	};

}