#include "Device.h"

#include "Vulkan.h"

#include <thread>
#include <chrono>
#include "RendererContext.h"

namespace vkPlayground::Renderer {

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////// Vulkan Physical Device
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	Ref<VulkanPhysicalDevice> VulkanPhysicalDevice::Create()
	{
		return CreateRef<VulkanPhysicalDevice>();
	}

	// TODO: Maybe switch this to work with like a high score system in case of multiple physical devices are available...?
	VulkanPhysicalDevice::VulkanPhysicalDevice()
	{
		VkInstance instance = RendererContext::GetInstance();

		uint32_t physicalDevicesCount;
		// Get number of available gpus
		vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, nullptr);
		PG_ASSERT(physicalDevicesCount > 0, "No gpus are found on the current device!");

		std::vector<VkPhysicalDevice> physicalDevices(physicalDevicesCount);
		// Enumerate devices
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, physicalDevices.data()));

		// If device is a discrete gpu directly select that!
		VkPhysicalDevice selectedDevice = nullptr;
		for (VkPhysicalDevice device : physicalDevices)
		{
			vkGetPhysicalDeviceProperties(device, &m_Properties);
			if (m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				selectedDevice = device;
				break;
			}
		}

		// If no discrete gpu found fallback to whatever physical device is found
		if (!selectedDevice)
		{
			std::cerr << "[Renderer] Could not find a discrete GPU!" << std::endl;
			selectedDevice = physicalDevices.back();
		}

		m_PhysicalDevice = selectedDevice;

		vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_Features);
		// TODO: Memory properties...!

		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
		PG_ASSERT(queueFamilyCount > 0, "");
		m_QueueFamilyProperties.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, m_QueueFamilyProperties.data());

		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);
		if (extensionCount > 0)
		{
			std::vector<VkExtensionProperties> props(extensionCount);
			if (vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, &props.front()) == VK_SUCCESS)
			{
				std::cout << "[Renderer] Selected Physical Device has " << extensionCount << " extensions...\n";
				for (const auto& ext : props)
				{
					m_SupportedExtensions.emplace(ext.extensionName);
					std::cout << "[Renderer] \t" << ext.extensionName << '\n';
				}
			}

		}

		// Queue Families
		// Desired Queues need to be requested upon logical device creation
		// Get queue family indices for the requested queue family types
		// Note that the indices may overlap depending on the implementation

		constexpr float DefaultQueuePriority = 0.0f;

		int requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
		m_QueueFamilyIndices = GetQueueFamilyIndices(requestedQueueTypes);

		// Graphics queue
		if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
		{
			VkDeviceQueueCreateInfo qCreateInfo = {};
			qCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			qCreateInfo.queueFamilyIndex = (uint32_t)m_QueueFamilyIndices.Graphics;
			qCreateInfo.queueCount = 1;
			qCreateInfo.pQueuePriorities = &DefaultQueuePriority;
			m_QueueCreateInfos.push_back(qCreateInfo);
		}

		// Dedicated Compute queue
		if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
		{
			// TODO:
		}

		// Dedicated Transfer queue
		if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
		{
			// TODO:
		}

		// TODO: Depth format
	}

	VulkanPhysicalDevice::~VulkanPhysicalDevice()
	{
	}

	bool VulkanPhysicalDevice::IsExtensionSupported(const std::string& extensionName) const
	{
		return m_SupportedExtensions.find(extensionName) != m_SupportedExtensions.end();
	}

	VulkanPhysicalDevice::QueueFamilyIndices VulkanPhysicalDevice::GetQueueFamilyIndices(int requestedIndices)
	{
		QueueFamilyIndices result;

		// TODO: For now not looking for completely separate queues (transfer, compute)
		// finds the first queue that supports the requested flags and returns it...
		for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); i++)
		{
			if (requestedIndices && VK_QUEUE_GRAPHICS_BIT)
			{
				if (m_QueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					result.Graphics = i;
				}
			}
		}

		return result;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////// Vulkan Device
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	Ref<VulkanDevice> VulkanDevice::Create(const Ref<VulkanPhysicalDevice>& physicalDevice, VkPhysicalDeviceFeatures enabledFeatures)
	{
		return CreateRef<VulkanDevice>(physicalDevice, enabledFeatures);
	}

	VulkanDevice::VulkanDevice(const Ref<VulkanPhysicalDevice>& physicalDevice, VkPhysicalDeviceFeatures enabledFeatures)
		: m_PhysicalDevice(physicalDevice), m_Features(enabledFeatures)
	{
		std::vector<const char*> deviceExtensions;
		// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
		PG_ASSERT(physicalDevice->IsExtensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME), "");
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		// TODO: Some more extensions to check and add...
		// TODO: Aftermath maybe???

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pNext = nullptr; // TODO: If we ever implement aftermath it will be passed into here...
		createInfo.queueCreateInfoCount = (uint32_t)physicalDevice->m_QueueCreateInfos.size();
		createInfo.pQueueCreateInfos = physicalDevice->m_QueueCreateInfos.data();
		createInfo.pEnabledFeatures = &enabledFeatures;

		// If a pNext(Chain) has been passed, we need to add it to the device creation info
		VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};

		if (physicalDevice->IsExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
		{
			deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
			m_DebugMarkersEnabled = true;
		}

		if (deviceExtensions.size() > 0)
		{
			createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
			createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		}

		VK_CHECK_RESULT(vkCreateDevice(m_PhysicalDevice->GetVulkanPhysicalDevice(), &createInfo, nullptr, &m_LogicalDevice));

		// TODO: CommandPools

		// Device Queues...
		vkGetDeviceQueue(m_LogicalDevice, m_PhysicalDevice->GetQueueFamilyIndices().Graphics, 0, &m_GraphicsQueue);
		//vkGetDeviceQueue(m_LogicalDevice, m_PhysicalDevice->GetQueueFamilyIndices().Compute, 0, &m_ComputeQueue);
	}

	VulkanDevice::~VulkanDevice()
	{
	}

	void VulkanDevice::Destroy()
	{
		vkDeviceWaitIdle(m_LogicalDevice);
		vkDestroyDevice(m_LogicalDevice, nullptr);
	}
}