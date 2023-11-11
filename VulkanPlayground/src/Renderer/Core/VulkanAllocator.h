#pragma once

#include "Renderer/Core/GPUMemoryStats.h"

#include <Vma/vk_mem_alloc.h>

#include <string_view>

namespace vkPlayground {

	class VulkanAllocator
	{
	public:
		VulkanAllocator() = default;
		VulkanAllocator(std::string_view name);
		~VulkanAllocator();

		static void Init();
		static void Shutdown();

		[[nodiscard]] VmaAllocation AllocateBuffer(const VkBufferCreateInfo* bufferCreateInfo, VmaMemoryUsage usage, VkBuffer* buffer);
		void DestroyBuffer(VmaAllocation allocation, VkBuffer buffer);
		
		template<typename T>
		[[nodiscard]]
		T* MapMemory(VmaAllocation allocation)
		{
			T* data;
			vmaMapMemory(GetVmaAllocator(), allocation, reinterpret_cast<void**>(&data));
			return data;
		}

		void UnmapMemory(VmaAllocation allocation);

		VmaAllocator& GetVmaAllocator();
		GPUMemoryStats GetStats();

	private:
		std::string_view m_Name;
	};

}