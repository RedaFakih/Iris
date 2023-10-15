#pragma once

#include "Core/Base.h"
#include "Renderer/Core/VulkanAllocator.h"

#include <vulkan/vulkan.h>

namespace vkPlayground {

	class IndexBuffer // : public RefCountedObject
	{
	public:
		// Create a buffer in DEVICE_LOCAL memory
		IndexBuffer(void* data, uint32_t size);
		// Create a buffer in HOST_VISIBLE memory which is not really usable for index buffers since index buffer are usually created once!
		IndexBuffer(uint32_t size);
		~IndexBuffer();

		[[nodiscard]] static Ref<IndexBuffer> Create(void* data, uint32_t size);
		[[nodiscard]] static Ref<IndexBuffer> Create(uint32_t size);

		// NOTE: Does not work if you created the buffer without giving it data directly which is not really advisable
		void SetData(void* data, uint32_t size, uint32_t offset = 0);

		// Default element size for the index buffer is a 32-bit unsigned int 
		uint32_t GetCount() const { return m_Size / sizeof(uint32_t); }
		uint32_t GetSize() const { return m_Size; }
		VkBuffer GetVulkanBuffer() const { return m_VulkanBuffer; }

	private:
		uint32_t m_Size = 0;

		VkBuffer m_VulkanBuffer = nullptr;
		VmaAllocation m_MemoryAllocation = nullptr;
	};

}