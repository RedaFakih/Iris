#include "IrisPCH.h"
#include "VertexBuffer.h"

#include "Renderer/Core/RendererContext.h"
#include "Renderer/Renderer.h"

namespace Iris {

	VertexBuffer::VertexBuffer(const void* data, uint32_t size, VertexBufferUsage usage)
		: m_Size(size)
	{
		(void)usage;
		m_LocalData = Buffer::Copy(reinterpret_cast<const uint8_t*>(data), size);

		Ref<VertexBuffer> instance = this;
		Renderer::Submit([instance]() mutable
		{
			Ref<VulkanDevice> device = RendererContext::GetCurrentDevice();
			VulkanAllocator allocator("VertexBuffer");

			VkBuffer stagingBuffer;
			VkBufferCreateInfo stagingInfo = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = instance->m_Size,
				.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE
			};

			VmaAllocation stagingBufferAlloc = allocator.AllocateBuffer(&stagingInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, &stagingBuffer);

			uint8_t* dstdata = allocator.MapMemory<uint8_t>(stagingBufferAlloc);
			std::memcpy(dstdata, instance->m_LocalData.Data, instance->m_LocalData.Size);
			allocator.UnmapMemory(stagingBufferAlloc);

			// We need to copy now
			VkBufferCreateInfo vertexBufferInfo = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = instance->m_Size,
				.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE // Exclusive to a single queue family
			};

			instance->m_MemoryAllocation = allocator.AllocateBuffer(&vertexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, &(instance->m_VulkanBuffer));

			VkCommandBuffer commandBuffer = device->GetCommandBuffer(true);

			VkBufferCopy copyRegion = {
				.srcOffset = 0,
				.dstOffset = 0,
				.size = instance->m_Size
			};

			vkCmdCopyBuffer(commandBuffer, stagingBuffer, instance->m_VulkanBuffer, 1, &copyRegion);

			device->FlushCommandBuffer(commandBuffer);

			allocator.DestroyBuffer(stagingBufferAlloc, stagingBuffer);
		});
	}

	VertexBuffer::VertexBuffer(uint32_t size, VertexBufferUsage usage)
		: m_Size(size)
	{
		(void)usage;
		m_LocalData.Allocate(size);

		Ref<VertexBuffer> instance = this;
		Renderer::Submit([instance]() mutable
		{
			VulkanAllocator allocator("VertexBuffer");

			VkBufferCreateInfo vertexBufferCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = instance->m_Size,
				.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE // Exclusive to a single queue family
			};

			instance->m_MemoryAllocation = allocator.AllocateBuffer(&vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, &(instance->m_VulkanBuffer));
		});
	}

	VertexBuffer::~VertexBuffer()
	{
		Renderer::SubmitReseourceFree([buffer = m_VulkanBuffer, allocation = m_MemoryAllocation]()
		{
			VulkanAllocator allocator("VertexBuffer");
			allocator.DestroyBuffer(allocation, buffer);
		});

		m_LocalData.Release();
	}

	void VertexBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		IR_ASSERT(size <= m_LocalData.Size);
		std::memcpy(m_LocalData.Data, reinterpret_cast<const uint8_t*>(data) + offset, size);
		Ref<VertexBuffer> instance = this;
		Renderer::Submit([instance, size, offset]() mutable
		{
			instance->RT_SetData(instance->m_LocalData.Data, size, offset);
		});
	}

	void VertexBuffer::RT_SetData(const void* data, uint32_t size, uint32_t offset)
	{
		IR_ASSERT(size <= m_Size, "Can't set more data than the buffer can hold!");

		VulkanAllocator allocator("VertexBuffer");

		uint8_t* dstData = allocator.MapMemory<uint8_t>(m_MemoryAllocation);
		std::memcpy(dstData, reinterpret_cast<const uint8_t*>(data) + offset, size);
		allocator.UnmapMemory(m_MemoryAllocation);
	}

}