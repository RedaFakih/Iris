#include "IndexBuffer.h"

#include "Renderer/Core/RendererContext.h"
#include "Renderer/Renderer.h"

namespace vkPlayground {

	/*
	 * TODO: Look at the IMPORTANT NOTE written in Renderer/Core/Device.h about using StagingBuffers in a better more sophisticated way
	 */

	IndexBuffer::IndexBuffer(void* data, uint32_t size)
		: m_Size(size)
	{
		Ref<VulkanDevice> device = RendererContext::GetCurrentDevice();
		VulkanAllocator allocator("IndexBuffer");

		VkBuffer stagingBuffer;
		VkBufferCreateInfo stagingBufferInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = size,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};
		VmaAllocation stagingBufferAllocation = allocator.AllocateBuffer(&stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, &stagingBuffer);

		uint8_t* dstData = allocator.MapMemory<uint8_t>(stagingBufferAllocation);
		std::memcpy(dstData, data, size);
		allocator.UnmapMemory(stagingBufferAllocation);

		// We need to copy now
		VkBufferCreateInfo indexBufferInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = size,
			.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE // Exclusive to a single queue family
		};
		m_MemoryAllocation = allocator.AllocateBuffer(&indexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, &m_VulkanBuffer);

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
		allocator.DestroyBuffer(stagingBufferAllocation, stagingBuffer);
	}

	IndexBuffer::IndexBuffer(uint32_t size)
		: m_Size(size)
	{
		PG_ASSERT(false, "Do you really wan't to do this? Index buffers are usually created once with their data...");
		PG_CORE_CRITICAL_TAG("Renderer", "Creating an IndexBuffer with just a `size`. Are you sure...??? IndexBuffers are usualy created only once!");

		VulkanAllocator allocator("IndexBuffer");

		VkBufferCreateInfo indexBufferInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = size,
			.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE // Exclusive to a single queue family
		};

		m_MemoryAllocation = allocator.AllocateBuffer(&indexBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, &m_VulkanBuffer);
	}

	IndexBuffer::~IndexBuffer()
	{
		Renderer::SubmitReseourceFree([indexBuffer = m_VulkanBuffer, allocation = m_MemoryAllocation]()
		{
			VulkanAllocator allocator("IndexBuffer");
			allocator.DestroyBuffer(allocation, indexBuffer);
		});
	}

	Ref<IndexBuffer> IndexBuffer::Create(void* data, uint32_t size)
	{
		return CreateRef<IndexBuffer>(data, size);
	}

	Ref<IndexBuffer> IndexBuffer::Create(uint32_t size)
	{
		return CreateRef<IndexBuffer>(size);
	}

	void IndexBuffer::SetData(void* data, uint32_t size, uint32_t offset)
	{
		PG_ASSERT(false, "Do you really wan't to do this? Index buffers are usually created once with their data...");
		PG_ASSERT(size <= m_Size, "Can't set more data than the buffer can hold!");
		PG_CORE_CRITICAL_TAG("Renderer", "Creating an IndexBuffer with just a `size`. Are you sure...??? IndexBuffers are usualy created only once!");

		VulkanAllocator allocator("IndexBuffer");

		uint8_t* dstData = allocator.MapMemory<uint8_t>(m_MemoryAllocation);
		std::memcpy(dstData, reinterpret_cast<uint8_t*>(data) + offset, size);
		allocator.UnmapMemory(m_MemoryAllocation);
	}

}