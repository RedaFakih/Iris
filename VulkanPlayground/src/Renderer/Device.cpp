#include "Device.h"

namespace vkPlayground::Renderer {

	Ref<VulkanPhysicalDevice> VulkanPhysicalDevice::Create()
	{
		return CreateRef<VulkanPhysicalDevice>();
	}

	VulkanPhysicalDevice::VulkanPhysicalDevice()
	{
	}

	VulkanPhysicalDevice::~VulkanPhysicalDevice()
	{

	}

	Ref<VulkanDevice> VulkanDevice::Create()
	{
		return CreateRef<VulkanDevice>();
	}

	VulkanDevice::VulkanDevice()
	{

	}

	VulkanDevice::~VulkanDevice()
	{

	}
}