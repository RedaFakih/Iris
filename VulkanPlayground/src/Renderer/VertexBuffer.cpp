#include "vkPch.h"
#include "VertexBuffer.h"

#include "Renderer/Core/RendererContext.h"
#include "Renderer/Renderer.h"

namespace vkPlayground {

	/*
	 * TODO: Look at the IMPORTANT NOTE written in Renderer/Core/Device.h about using StagingBuffers in a better more sophisticated way
	 */

	VertexBuffer::VertexBuffer(void* data, uint32_t size, VertexBufferUsage usage)
		: m_Size(size)
	{
		Ref<VulkanDevice> device = RendererContext::GetCurrentDevice();
		VulkanAllocator allocator("VertexBuffer");

		VkBuffer stagingBuffer;
		VkBufferCreateInfo stagingInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = size,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};
		
		VmaAllocation stagingBufferAlloc = allocator.AllocateBuffer(&stagingInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, &stagingBuffer);

		uint8_t* dstdata = allocator.MapMemory<uint8_t>(stagingBufferAlloc);
		std::memcpy(dstdata, data, size);
		allocator.UnmapMemory(stagingBufferAlloc);

		// We need to copy now
		VkBufferCreateInfo vertexBufferInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = size,
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE // Exclusive to a single queue family
		};

		m_MemoryAllocation = allocator.AllocateBuffer(&vertexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, &m_VulkanBuffer);

		// TODO: Refer to the note in Renderer/Core/Device.h since maybe we could just begin and return a pre-allocated buffer?
		VkCommandBuffer commandBuffer = device->GetCommandBuffer(true);

		VkBufferCopy copyRegion = {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = size
		};

		vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_VulkanBuffer, 1, &copyRegion);
		
		// TODO: Refer to the note in Renderer/Core/Device.h since we do not want to do this really...
		device->FlushCommandBuffer(commandBuffer);

		// NOTE: If we implement the note written in Device.h we would defer this cleanup to the beginning of the next frame.
		// Renderer::SubmitResourceFree
		allocator.DestroyBuffer(stagingBufferAlloc, stagingBuffer);
	}

	VertexBuffer::VertexBuffer(uint32_t size, VertexBufferUsage usage)
		: m_Size(size)
	{
		VulkanAllocator allocator("VertexBuffer");

		VkBufferCreateInfo vertexBufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = size,
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE // Exclusive to a single queue family
		};

		m_MemoryAllocation = allocator.AllocateBuffer(&vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, &m_VulkanBuffer);
	}

	VertexBuffer::~VertexBuffer()
	{
		Renderer::SubmitReseourceFree([buffer = m_VulkanBuffer, allocation = m_MemoryAllocation]()
		{
			VulkanAllocator allocator("VertexBuffer");
			allocator.DestroyBuffer(allocation, buffer);
		});
	}

	Ref<VertexBuffer> VertexBuffer::Create(void* data, uint32_t size, VertexBufferUsage usage)
	{
		return CreateRef<VertexBuffer>(data, size, usage);
	}

	Ref<VertexBuffer> VertexBuffer::Create(uint32_t size, VertexBufferUsage usage)
	{
		return CreateRef<VertexBuffer>(size, usage);
	}

	void VertexBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		VKPG_ASSERT(size <= m_Size, "Can't set more data than the buffer can hold!");

		VulkanAllocator allocator("VertexBuffer");

		uint8_t* dstData = allocator.MapMemory<uint8_t>(m_MemoryAllocation);
		std::memcpy(dstData, reinterpret_cast<const uint8_t*>(data) + offset, size);
		allocator.UnmapMemory(m_MemoryAllocation);
	}

}