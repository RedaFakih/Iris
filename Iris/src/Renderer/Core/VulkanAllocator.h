#pragma once

#include "Renderer/Core/GPUMemoryStats.h"

#include <Vma/vk_mem_alloc.h>

#include <string_view>

namespace Iris {

	class VulkanAllocator
	{
	public:
		VulkanAllocator() = default;
		VulkanAllocator(std::string_view name);
		~VulkanAllocator();

		static void Init();
		static void Shutdown();

		[[nodiscard]] VmaAllocation AllocateBuffer(const VkBufferCreateInfo* bufferCreateInfo, VmaMemoryUsage usage, VkBuffer* buffer, VmaAllocationInfo* allocationInfo = nullptr);
		void DestroyBuffer(VmaAllocation allocation, VkBuffer buffer);

		[[nodiscard]] VmaAllocation AllocateImage(const VkImageCreateInfo* imageCreateInfo, VmaMemoryUsage usage, VkImage* image, VkDeviceSize* allocatedSize = nullptr);
		void DestroyImage(VmaAllocation allocation, VkImage image);
		
		template<typename T>
		[[nodiscard]]
		T* MapMemory(VmaAllocation allocation)
		{
			T* data;
			vmaMapMemory(GetVmaAllocator(), allocation, reinterpret_cast<void**>(&data));
			return data;
		}

		void UnmapMemory(VmaAllocation allocation);

		VmaAllocationInfo GetAllocationInfo(VmaAllocation allocation) const;

		static VmaAllocator& GetVmaAllocator();
		static GPUMemoryStats GetStats();
		static void DumpStats();

	private:
		std::string_view m_Name;

	};

}