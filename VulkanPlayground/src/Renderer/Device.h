#pragma once

#include "Base.h"

#include <vulkan/vulkan.h>

namespace vkPlayground::Renderer {

	class VulkanPhysicalDevice
	{
	public:
		VulkanPhysicalDevice();
		~VulkanPhysicalDevice();

		static Ref<VulkanPhysicalDevice> Create();

	private:
		VkPhysicalDevice m_PhysicalDevice = nullptr;

	};

	// Vulkan Logical Device
	class VulkanDevice
	{
	public:
		VulkanDevice();
		~VulkanDevice();

		static Ref<VulkanDevice> Create();

	private:
		VkDevice m_Device = nullptr;

	};

}