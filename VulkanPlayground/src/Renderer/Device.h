#pragma once

#include "Core/Base.h"

#include <vulkan/vulkan.h>

#include <vector>
#include <unordered_set>

namespace vkPlayground::Renderer {

	class VulkanPhysicalDevice
	{
	public:
		struct QueueFamilyIndices
		{
			int32_t Graphics = -1;
		};

	public:
		VulkanPhysicalDevice();
		~VulkanPhysicalDevice();

		static Ref<VulkanPhysicalDevice> Create();

		VkPhysicalDevice GetVulkanPhysicalDevice() const { return m_PhysicalDevice; }
		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }

		const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties() const { return m_Properties; }
		const VkPhysicalDeviceLimits& GetPhysicalDeviceLimits() const { return m_Properties.limits; }
		const VkPhysicalDeviceFeatures& GetPhysicalDeviceFeatures() const { return m_Features; }

		bool IsExtensionSupported(const std::string& extensionName) const;

	private:
		QueueFamilyIndices GetQueueFamilyIndices(int requestedIndices);

	private:
		// This will be implicitely destroyed when the VkInstance gets destroyed
		VkPhysicalDevice m_PhysicalDevice = nullptr;
		VkPhysicalDeviceProperties m_Properties;
		VkPhysicalDeviceFeatures m_Features;

		QueueFamilyIndices m_QueueFamilyIndices;

		std::vector<VkQueueFamilyProperties> m_QueueFamilyProperties;
		std::vector<VkDeviceQueueCreateInfo> m_QueueCreateInfos;
		std::unordered_set<std::string> m_SupportedExtensions;

		friend class VulkanDevice;

	};

	// Vulkan Logical Device
	class VulkanDevice
	{
	public:
		VulkanDevice(const Ref<VulkanPhysicalDevice>& physicalDevice, VkPhysicalDeviceFeatures enabledFeatures);
		~VulkanDevice();

		static Ref<VulkanDevice> Create(const Ref<VulkanPhysicalDevice>& physicalDevice, VkPhysicalDeviceFeatures enabledFeatures);

		void Destroy();

		VkQueue GetGraphicsQueue() { return m_GraphicsQueue; }
		//VkQueue GetComputeQueue() { return m_ComputeQueue; }

		const Ref<VulkanPhysicalDevice> GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice GetVulkanDevice() const { return m_LogicalDevice; }

	private:
		VkDevice m_LogicalDevice = nullptr;
		Ref<VulkanPhysicalDevice> m_PhysicalDevice = nullptr;
		VkPhysicalDeviceFeatures m_Features;

		VkQueue m_GraphicsQueue;
		// VkQueue m_ComputeQueue;

		bool m_DebugMarkersEnabled = false;

	};

}