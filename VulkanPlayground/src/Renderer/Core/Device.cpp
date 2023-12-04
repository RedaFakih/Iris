#include "vkPch.h"
#include "Device.h"

#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/Vulkan.h"

namespace vkPlayground {

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////// Vulkan Physical Device
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	Ref<VulkanPhysicalDevice> VulkanPhysicalDevice::Create()
	{
		return CreateRef<VulkanPhysicalDevice>();
	}

	VulkanPhysicalDevice::VulkanPhysicalDevice()
	{
		VkInstance instance = RendererContext::GetInstance();

		uint32_t gpuCount;
		// Get number of available gpus
		vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
		PG_ASSERT(gpuCount > 0, "No gpus are found on the current device!");

		std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
		// Enumerate devices
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data()));

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
			PG_CORE_ERROR_TAG("Renderer", "Could not find a discrete GPU!");
			selectedDevice = physicalDevices.back();
		}

		PG_ASSERT(selectedDevice, "Could find any physical devices!");
		m_PhysicalDevice = selectedDevice;

		vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_Features);
		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &m_MemoryProperties);

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
				PG_CORE_WARN_TAG("Renderer", "Selected Physical Device has: {0} extensions...", extensionCount);
				for (const auto& ext : props)
				{
					m_SupportedExtensions.emplace(ext.extensionName);
					PG_CORE_TRACE_TAG("Renderer", "\t{0}", ext.extensionName);
				}
			}

		}

		// Queue Families
		// Desired Queues need to be requested upon logical device creation
		// Get queue family indices for the requested queue family types
		// Note that the indices may overlap depending on the implementation

		static constexpr float DefaultQueuePriority = 0.0f;

		int requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
		m_QueueFamilyIndices = GetQueueFamilyIndices(requestedQueueTypes);

		// Graphics queue
		if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
		{
			VkDeviceQueueCreateInfo qCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = (uint32_t)m_QueueFamilyIndices.Graphics,
				.queueCount = 1,
				.pQueuePriorities = &DefaultQueuePriority
			};
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

		m_DepthFormat = FindDepthFormat();
		PG_ASSERT(m_DepthFormat, "Need to have a default depth format!");
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
		
		if (requestedIndices & VK_QUEUE_GRAPHICS_BIT)
		{
			for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); i++)
			{
				if (m_QueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					result.Graphics = i;
				}
			}
		}

		// TODO: Do the same for the other VK_QUEUE_XXX_BIT (Transfer, Compute)

		return result;
	}

	VkFormat VulkanPhysicalDevice::FindDepthFormat() const
	{
		// Since all depth formats might be optional, we need to find the most suitable one starting from the highest precision packed format
		std::array<VkFormat, 5> candidates = {
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM
		};

		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);

			// Format must support depth and stencil attachment for optimal tiling
			if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				return format;
		}

		return VK_FORMAT_UNDEFINED;
	}

	// Used when we want to get the memory properties of a buffer and find memory type for `VkMemoryAllocateInfo`
	uint32_t VulkanPhysicalDevice::FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags props) const
	{
		for (uint32_t i = 0; i < m_MemoryProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (m_MemoryProperties.memoryTypes[i].propertyFlags & props) == props)
				return i;
		}

		PG_ASSERT(false, "Could not find a suitable memory type!");
		return UINT32_MAX;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////// Vulkan Device
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	Ref<VulkanDevice> VulkanDevice::Create(const Ref<VulkanPhysicalDevice>& physicalDevice, const VkPhysicalDeviceFeatures& enabledFeatures)
	{
		return CreateRef<VulkanDevice>(physicalDevice, enabledFeatures);
	}

	VulkanDevice::VulkanDevice(const Ref<VulkanPhysicalDevice>& physicalDevice, const VkPhysicalDeviceFeatures& enabledFeatures)
		: m_PhysicalDevice(physicalDevice), m_Features(enabledFeatures)
	{
		std::vector<const char*> deviceExtensions;
		// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
		PG_ASSERT(physicalDevice->IsExtensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME), "");
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		// TODO: Some more extensions to check and add Aftermath maybe..??

		VkDeviceCreateInfo createInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = nullptr, // TODO: If we ever implement aftermath it will be passed into here...
			.queueCreateInfoCount = (uint32_t)physicalDevice->m_QueueCreateInfos.size(),
			.pQueueCreateInfos = physicalDevice->m_QueueCreateInfos.data(),
			.pEnabledFeatures = &enabledFeatures
		};

		// NOTE: Debug Labels (formerly known as Markers) are implicitely supported with the VK_EXT_debug_utils extension...
		
		if (deviceExtensions.size() > 0)
		{
			createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
			createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		}

		VK_CHECK_RESULT(vkCreateDevice(m_PhysicalDevice->GetVulkanPhysicalDevice(), &createInfo, nullptr, &m_LogicalDevice));

		// Device Queues...
		vkGetDeviceQueue(m_LogicalDevice, m_PhysicalDevice->GetQueueFamilyIndices().Graphics, 0, &m_GraphicsQueue);
		// TODO: vkGetDeviceQueue(m_LogicalDevice, m_PhysicalDevice->GetQueueFamilyIndices().Compute, 0, &m_ComputeQueue);
	}

	VulkanDevice::~VulkanDevice()
	{
	}

	void VulkanDevice::Destroy()
	{
		// Need to destroy all the commandpools before destroying the vulkan device
		m_CommandPools.clear();
		
		vkDeviceWaitIdle(m_LogicalDevice);
		vkDestroyDevice(m_LogicalDevice, nullptr);
	}

	VkCommandBuffer VulkanDevice::GetCommandBuffer(bool begin/* , bool compute */)
	{
		return GetOrCreateThreadLocalCommandPool()->AllocateCommandBuffer(begin/* , compute */);
	}

	void VulkanDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer)
	{
		GetThreadLocalCommandPool()->FlushCommandBuffer(commandBuffer);
	}

	void VulkanDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue)
	{
		GetThreadLocalCommandPool()->FlushCommandBuffer(commandBuffer, queue);
	}

	Ref<VulkanCommandPool> VulkanDevice::GetThreadLocalCommandPool()
	{
		std::thread::id threadID = std::this_thread::get_id();
		PG_ASSERT(m_CommandPools.find(threadID) != m_CommandPools.end(), "Not Found!");

		return m_CommandPools.at(threadID);
	}

	Ref<VulkanCommandPool> VulkanDevice::GetOrCreateThreadLocalCommandPool()
	{
		std::thread::id threadID = std::this_thread::get_id();
		auto it = m_CommandPools.find(threadID);
		if (it != m_CommandPools.end())
			return it->second;

		Ref<VulkanCommandPool> commandPool = VulkanCommandPool::Create();
		m_CommandPools[threadID] = commandPool;
		return commandPool;
	}

	//////////////////////////////////////////////////////////////////////////
	/// CommandPool
	//////////////////////////////////////////////////////////////////////////

	VulkanCommandPool::VulkanCommandPool()
	{
		Ref<VulkanDevice> device = RendererContext::GetCurrentDevice();
		VkDevice vulkanDevice = device->GetVulkanDevice();

		VkCommandPoolCreateInfo commandPoolInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, 
			.queueFamilyIndex = (uint32_t)device->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics,
		};

		VK_CHECK_RESULT(vkCreateCommandPool(vulkanDevice, &commandPoolInfo, nullptr, &m_GraphicsCommandPool));

		// TODO: Also for m_ComputeCommandPool
	}

	VulkanCommandPool::~VulkanCommandPool()
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroyCommandPool(device, m_GraphicsCommandPool, nullptr);
		// TODO: Also for m_ComputeCommandPool
	}

	Ref<VulkanCommandPool> VulkanCommandPool::Create()
	{
		return CreateRef<VulkanCommandPool>();
	}

	VkCommandBuffer VulkanCommandPool::AllocateCommandBuffer(bool begin/* , bool compute */)
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

		VkCommandBufferAllocateInfo allocateInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = m_GraphicsCommandPool, // compute ? m_ComputeCommandPool : m_GraphicsCommandPool
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		VkCommandBuffer commandBuffer;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer));

		// In case we want to start the new command buffer directly
		if (begin)
		{
			VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo));
		}

		return commandBuffer;
	}

	void VulkanCommandPool::FlushCommandBuffer(VkCommandBuffer commandBuffer)
	{
		Ref<VulkanDevice> device = RendererContext::GetCurrentDevice();
		FlushCommandBuffer(commandBuffer, device->GetGraphicsQueue());
	}

	void VulkanCommandPool::FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue)
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

		PG_ASSERT(commandBuffer != VK_NULL_HANDLE, "Can't flush a null buffer");

		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer
		};

		VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		VkFence fence;
		VK_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &fence));

		// Not more than one thread can submit to the SAME queue at the same time since that is UB
		{
			static std::mutex submissionLock;
			std::scoped_lock<std::mutex> lock(submissionLock);

			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
		}

		VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

		vkDestroyFence(device, fence, nullptr);
		vkFreeCommandBuffers(device, m_GraphicsCommandPool, 1, &commandBuffer);
	}
}