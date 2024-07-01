#include "IrisPCH.h"
#include "Device.h"

#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/Vulkan.h"
#include "Renderer/Renderer.h"

namespace Iris {

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
		IR_VERIFY(gpuCount > 0, "No gpus are found on the current device!");

		std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
		// Enumerate devices
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data()));

		// If device is a discrete gpu directly select that!
		VkPhysicalDevice selectedDevice = nullptr;
		for (VkPhysicalDevice device : physicalDevices)
		{
			m_Properties = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
			};

			vkGetPhysicalDeviceProperties2(device, &m_Properties);
			if (m_Properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				selectedDevice = device;
				break;
			}
		}

		// If no discrete gpu found fallback to whatever physical device is found
		if (!selectedDevice)
		{
			IR_CORE_ERROR_TAG("Renderer", "Could not find a discrete GPU!");
			selectedDevice = physicalDevices.back();
		}

		IR_VERIFY(selectedDevice, "Could find any physical devices!");
		m_PhysicalDevice = selectedDevice;

		m_Features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = nullptr
		};
		vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &m_Features);

		m_MemoryProperties = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
			.pNext = nullptr
		};
		vkGetPhysicalDeviceMemoryProperties2(m_PhysicalDevice, &m_MemoryProperties);

		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
		IR_ASSERT(queueFamilyCount > 0);
		m_QueueFamilyProperties.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, m_QueueFamilyProperties.data());

		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);
		if (extensionCount > 0)
		{
			std::vector<VkExtensionProperties> props(extensionCount);
			if (vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, &props.front()) == VK_SUCCESS)
			{
				IR_CORE_WARN_TAG("Renderer", "Selected Physical Device has: {0} extensions...", extensionCount);
				for (const auto& ext : props)
				{
					m_SupportedExtensions.emplace(ext.extensionName);
					IR_CORE_TRACE_TAG("Renderer", "\t{0}", ext.extensionName);
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
				.queueFamilyIndex = static_cast<uint32_t>(m_QueueFamilyIndices.Graphics),
				.queueCount = 1,
				.pQueuePriorities = &DefaultQueuePriority
			};
			m_QueueCreateInfos.push_back(qCreateInfo);
		}

		// Dedicated Compute queue
		if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			if (m_QueueFamilyIndices.Compute != m_QueueFamilyIndices.Graphics)
			{
				VkDeviceQueueCreateInfo qCI = {
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = static_cast<uint32_t>(m_QueueFamilyIndices.Compute),
					.queueCount = 1,
					.pQueuePriorities = &DefaultQueuePriority
				};
				m_QueueCreateInfos.push_back(qCI);
			}
		}

		// Dedicated Transfer queue
		if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
		{
			// If transfer family index differs from both compute and graphics, we need an additional queue for the transfer queue
			if ((m_QueueFamilyIndices.Transfer != m_QueueFamilyIndices.Compute) && (m_QueueFamilyIndices.Transfer != m_QueueFamilyIndices.Graphics))
			{
				VkDeviceQueueCreateInfo qCI = {
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = static_cast<uint32_t>(m_QueueFamilyIndices.Transfer),
					.queueCount = 1,
					.pQueuePriorities = &DefaultQueuePriority
				};
				m_QueueCreateInfos.push_back(qCI);
			}
		}

		m_DepthFormat = FindDepthFormat();
		IR_VERIFY(m_DepthFormat, "Need to have a default depth format!");
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

		// Dedicated queue for compute
		// Try to find a queue family index that supports compute but not graphics
		if (requestedIndices & VK_QUEUE_COMPUTE_BIT)
		{
			for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); i++)
			{
				auto& queueFamilyProps = m_QueueFamilyProperties[i];
				if ((queueFamilyProps.queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queueFamilyProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
				{
					result.Compute = i;
					break;
				}
			}
		}

		// Dedicated queue for transfer
		// Try to find a queue family index that supports transfer but not graphics or compute
		if (requestedIndices & VK_QUEUE_TRANSFER_BIT)
		{
			for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); i++)
			{
				auto& queueFamilyProps = m_QueueFamilyProperties[i];
				if ((queueFamilyProps.queueFlags & VK_QUEUE_TRANSFER_BIT) && ((queueFamilyProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProps.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
				{
					result.Transfer = i;
					break;
				}
			}
		}

		// For other queue types or if no separate queue is present, return the first one to support the requested flags
		for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); i++)
		{
			if ((requestedIndices & VK_QUEUE_TRANSFER_BIT) && result.Transfer == -1)
			{
				if (m_QueueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
					result.Transfer = i;
			}

			if ((requestedIndices & VK_QUEUE_COMPUTE_BIT) && result.Compute == -1)
			{
				if (m_QueueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
					result.Compute = i;
			}

			if (requestedIndices & VK_QUEUE_GRAPHICS_BIT)
			{
				if (m_QueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
					result.Graphics = i;
			}
		}

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
		for (uint32_t i = 0; i < m_MemoryProperties.memoryProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (m_MemoryProperties.memoryProperties.memoryTypes[i].propertyFlags & props) == props)
				return i;
		}

		IR_ASSERT(false, "Could not find a suitable memory type!");
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
		IR_VERIFY(physicalDevice->IsExtensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME));
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		// TODO: Some more extensions to check and add Aftermath maybe..??

		VkPhysicalDeviceSynchronization2Features synchronization2Feature = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
			.pNext = nullptr,
			.synchronization2 = VK_TRUE
		};

		VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
			.pNext = &synchronization2Feature,
			.dynamicRendering = VK_TRUE
		};

		VkDeviceCreateInfo createInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &dynamicRenderingFeature,
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
		VKUtils::SetDebugUtilsObjectName(m_LogicalDevice, VK_OBJECT_TYPE_QUEUE, "GraphicsQueue", m_GraphicsQueue);
		vkGetDeviceQueue(m_LogicalDevice, m_PhysicalDevice->GetQueueFamilyIndices().Compute, 0, &m_ComputeQueue);
		VKUtils::SetDebugUtilsObjectName(m_LogicalDevice, VK_OBJECT_TYPE_QUEUE, "ComputeQueue", m_ComputeQueue);
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

	void VulkanDevice::LockQueue(bool compute)
	{
		if (compute)
			m_ComputeQueueMutex.lock();
		else
			m_GraphicsQueueMutex.lock();
	}

	void VulkanDevice::UnlockQueue(bool compute)
	{
		if (compute)
			m_ComputeQueueMutex.unlock();
		else
			m_GraphicsQueueMutex.unlock();
	}

	VkCommandBuffer VulkanDevice::GetCommandBuffer(bool begin, bool compute)
	{
		return GetOrCreateThreadLocalCommandPool()->AllocateCommandBuffer(begin, compute);
	}

	void VulkanDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer, bool compute)
	{
		GetThreadLocalCommandPool()->FlushCommandBuffer(commandBuffer, compute);
	}

	void VulkanDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue)
	{
		GetThreadLocalCommandPool()->FlushCommandBuffer(commandBuffer, queue);
	}

	Ref<VulkanCommandPool> VulkanDevice::GetThreadLocalCommandPool()
	{
		std::thread::id threadID = std::this_thread::get_id();
		IR_ASSERT(m_CommandPools.contains(threadID), "Not found!");

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
			.queueFamilyIndex = static_cast<uint32_t>(device->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics),
		};

		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		m_GraphicsCommandPools.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
			VK_CHECK_RESULT(vkCreateCommandPool(vulkanDevice, &commandPoolInfo, nullptr, &m_GraphicsCommandPools[i]));

		commandPoolInfo.queueFamilyIndex = static_cast<uint32_t>(device->GetPhysicalDevice()->GetQueueFamilyIndices().Compute);
		m_ComputeCommandPools.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
			VK_CHECK_RESULT(vkCreateCommandPool(vulkanDevice, &commandPoolInfo, nullptr, &m_ComputeCommandPools[i]));
	}

	VulkanCommandPool::~VulkanCommandPool()
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

		for (uint32_t i = 0; i < m_GraphicsCommandPools.size(); i++)
			vkDestroyCommandPool(device, m_GraphicsCommandPools[i], nullptr);

		for (uint32_t i = 0; i < m_ComputeCommandPools.size(); i++)
			vkDestroyCommandPool(device, m_ComputeCommandPools[i], nullptr);
	}

	Ref<VulkanCommandPool> VulkanCommandPool::Create()
	{
		return CreateRef<VulkanCommandPool>();
	}

	void VulkanCommandPool::Reset()
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		vkResetCommandPool(device, m_GraphicsCommandPools[frameIndex], 0);
		vkResetCommandPool(device, m_ComputeCommandPools[frameIndex], 0);
	}

	VkCommandBuffer VulkanCommandPool::AllocateCommandBuffer(bool begin, bool compute)
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		VkCommandBufferAllocateInfo allocateInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = compute ? m_ComputeCommandPools[frameIndex] : m_GraphicsCommandPools[frameIndex],
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		VkCommandBuffer commandBuffer;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer));

		// In case we want to start the new command buffer directly
		if (begin)
		{
			VkCommandBufferBeginInfo beginInfo = { 
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo));
		}

		return commandBuffer;
	}

	void VulkanCommandPool::FlushCommandBuffer(VkCommandBuffer commandBuffer, bool compute)
	{
		Ref<VulkanDevice> device = RendererContext::GetCurrentDevice();
		FlushCommandBuffer(commandBuffer, compute ? device->GetComputeQueue() : device->GetGraphicsQueue());
	}

	void VulkanCommandPool::FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue)
	{
		Ref<VulkanDevice> device = RendererContext::GetCurrentDevice();
		VkDevice vulkanDevice = device->GetVulkanDevice();

		IR_ASSERT(commandBuffer != VK_NULL_HANDLE, "Can't flush a null buffer");
		IR_ASSERT(queue != VK_NULL_HANDLE, "No Queue provided!");

		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer
		};

		VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		VkFence fence;
		VK_CHECK_RESULT(vkCreateFence(vulkanDevice, &fenceInfo, nullptr, &fence));

		// Not more than one thread can submit to the SAME queue at the same time since that is UB
		{
			device->LockQueue();

			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));

			device->UnlockQueue();
		}

		VK_CHECK_RESULT(vkWaitForFences(vulkanDevice, 1, &fence, VK_TRUE, UINT64_MAX));

		vkDestroyFence(vulkanDevice, fence, nullptr);
	}
}