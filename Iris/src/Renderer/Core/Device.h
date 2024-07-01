#pragma once

#include "Core/Base.h"

#include <vulkan/vulkan.h>

#include <map>
#include <unordered_set>
#include <vector>

namespace Iris {

	class VulkanPhysicalDevice : public RefCountedObject
	{
	public:
		struct QueueFamilyIndices
		{
			int32_t Graphics = -1;
			int32_t Compute = -1;
			int32_t Transfer = -1;
		};

	public:
		VulkanPhysicalDevice();
		~VulkanPhysicalDevice();

		[[nodiscard]] static Ref<VulkanPhysicalDevice> Create();

		VkPhysicalDevice GetVulkanPhysicalDevice() const { return m_PhysicalDevice; }
		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }

		const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties() const { return m_Properties.properties; }
		const VkPhysicalDeviceMemoryProperties& GetPhysicalDeviceMemoryProps() const { return m_MemoryProperties.memoryProperties; }
		const VkPhysicalDeviceLimits& GetPhysicalDeviceLimits() const { return m_Properties.properties.limits; }
		const VkPhysicalDeviceFeatures& GetPhysicalDeviceFeatures() const { return m_Features.features; }

		bool IsExtensionSupported(const std::string& extensionName) const;
		uint32_t FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags props) const;

		VkFormat GetDepthFormat() const { return m_DepthFormat; }

	private:
		QueueFamilyIndices GetQueueFamilyIndices(int requestedIndices);
		VkFormat FindDepthFormat() const;

	private:
		// This will be implicitely destroyed when the VkInstance gets destroyed
		VkPhysicalDevice m_PhysicalDevice = nullptr;
		VkPhysicalDeviceProperties2 m_Properties;
		VkPhysicalDeviceFeatures2 m_Features;
		VkPhysicalDeviceMemoryProperties2 m_MemoryProperties;

		QueueFamilyIndices m_QueueFamilyIndices;

		std::vector<VkQueueFamilyProperties> m_QueueFamilyProperties;
		std::vector<VkDeviceQueueCreateInfo> m_QueueCreateInfos;

		std::unordered_set<std::string> m_SupportedExtensions;

		VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;

		friend class VulkanDevice;

	};

	/*
	 * This is mainly for when we want to quickly allocate and then destroy a commandbuffer to submit a couple of small commands
	 * This is an interface and VulkanDevice provides the direct API to use
	 * 
	 * TODO(IMPORTANT): `This is especially for Staging Buffers and handling them`
	 *   The note mentioned above is actually very NOT optimal and NOT good since it calls a `vkQueueSubmit` on a small commandbuffer
	 *	 And as for Staging Buffers we currently allocate a command buffer, record copy command and then `vkQueueSubmit` which, as mentioned,
	 *	 is NOT that good of a thing concerning performance.
	 *	 To fix that we can(should): Create a command buffer (beginning of application maybe?) that records copy commands for staging buffers
	 *	 mainly (and maybe other stuff like that) and then at the next call for `vkQueueSubmit`, whether that is at the end of the frame or
	 *   not (To be handled in case we have a `vkQueueSubmit` not at the of the frame) we submit that command buffer to the command buffer
	 *   array of the `VkSubmitInfo` and we make sure to submit it as the first command buffer to execute so that the data is available for
	 *   when the next command buffer is to be executed.
	 * 
	 *   As for deleting the Staging Buffer we could just do a `Renderer::SubmitReseourceFree` and it will delete the staging buffer at the
	 *   beginning of the next frame that way nothing is using that buffer.
	 * 
	 *   Potential Obstacles: Copying attachments to host buffers...? We will have to queue the copying to host buffer to the next frame so that the data is available
	 *
	 *   Provide a flag that allows us to instantly submit that command buffer or queue it up until the next vkQueueSubmit
	 */
	class VulkanCommandPool : public RefCountedObject
	{
	public:
		VulkanCommandPool();
		~VulkanCommandPool();

		[[nodiscard]] static Ref<VulkanCommandPool> Create();

		void Reset();

		VkCommandBuffer AllocateCommandBuffer(bool begin, bool compute = false);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer, bool compute = false);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue);

		VkCommandPool GetGraphicsCommandPool(uint32_t frameIndex) const { IR_ASSERT(frameIndex < m_GraphicsCommandPools.size()); return m_GraphicsCommandPools[frameIndex]; }
		VkCommandPool GetComputeCommandPool(uint32_t frameIndex) const { IR_ASSERT(frameIndex < m_GraphicsCommandPools.size(), ""); return m_ComputeCommandPools[frameIndex]; }

	private:
		// Per-frame command pools to get better perf
		std::vector<VkCommandPool> m_GraphicsCommandPools;
		std::vector<VkCommandPool> m_ComputeCommandPools;
	};

	// Vulkan Logical Device
	class VulkanDevice : public RefCountedObject
	{
	public:
		VulkanDevice(const Ref<VulkanPhysicalDevice>& physicalDevice, const VkPhysicalDeviceFeatures& enabledFeatures);
		~VulkanDevice();

		[[nodiscard]] static Ref<VulkanDevice> Create(const Ref<VulkanPhysicalDevice>& physicalDevice, const VkPhysicalDeviceFeatures& enabledFeatures);

		void Destroy();

		void LockQueue(bool compute = false);
		void UnlockQueue(bool compute = false);

		VkQueue GetGraphicsQueue() { return m_GraphicsQueue; }
		VkQueue GetComputeQueue() { return m_ComputeQueue; }

		VkCommandBuffer GetCommandBuffer(bool begin, bool compute = false);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer, bool compute = false);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue);

		const Ref<VulkanPhysicalDevice> GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice GetVulkanDevice() const { return m_LogicalDevice; }

	private:
		Ref<VulkanCommandPool> GetThreadLocalCommandPool();
		Ref<VulkanCommandPool> GetOrCreateThreadLocalCommandPool();

	private:
		VkDevice m_LogicalDevice = nullptr;
		Ref<VulkanPhysicalDevice> m_PhysicalDevice = nullptr;
		VkPhysicalDeviceFeatures m_Features;

		VkQueue m_GraphicsQueue;
		VkQueue m_ComputeQueue;

		std::map<std::thread::id, Ref<VulkanCommandPool>> m_CommandPools;

		std::mutex m_GraphicsQueueMutex;
		std::mutex m_ComputeQueueMutex;

		friend class Renderer;
	};

}