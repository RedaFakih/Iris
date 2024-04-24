#include "IrisPCH.h"
#include "UniformBuffer.h"

#include "Renderer.h"

namespace Iris {

	UniformBuffer::UniformBuffer(size_t size)
		: m_Size(size)
	{
		m_LocalData.Allocate(size);

		VkBufferCreateInfo bufferInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.size = m_Size,
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		};

		VulkanAllocator allocator("UnifromBuffer");
		m_MemoryAllocation = allocator.AllocateBuffer(&bufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, &m_VulkanBuffer);

		m_DescriptorInfo.buffer = m_VulkanBuffer;
		m_DescriptorInfo.offset = 0;
		m_DescriptorInfo.range = m_Size;
	}

	UniformBuffer::~UniformBuffer()
	{
		Renderer::SubmitReseourceFree([buffer = m_VulkanBuffer, alloc = m_MemoryAllocation]()
		{
			VulkanAllocator allocator("UniformBuffer");
			allocator.DestroyBuffer(alloc, buffer);
		});

		m_LocalData.Release();
		m_MemoryAllocation = nullptr;
		m_VulkanBuffer = nullptr;
	}

	Ref<UniformBuffer> UniformBuffer::Create(size_t size)
	{
		return CreateRef<UniformBuffer>(size);
	}

	void UniformBuffer::SetData(const void* data, size_t size, size_t offset)
	{
		std::memcpy(m_LocalData.Data, data, size); // Copy all the data here and then in the other function account for the offset
		Ref<UniformBuffer> instance = this;
		Renderer::Submit([instance, size, offset]() mutable
		{
			instance->RT_SetData(instance->m_LocalData.Data, size, offset);
		});
	}

	void UniformBuffer::RT_SetData(const void* data, size_t size, size_t offset)
	{
		VulkanAllocator allocator("UniformBuffer");

		uint8_t* dstData = allocator.MapMemory<uint8_t>(m_MemoryAllocation);
		std::memcpy(dstData, reinterpret_cast<const uint8_t*>(data) + offset, size);
		allocator.UnmapMemory(m_MemoryAllocation);
	}

}