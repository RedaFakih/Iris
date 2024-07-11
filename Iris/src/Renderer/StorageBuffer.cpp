#include "IrisPCH.h"
#include "StorageBuffer.h"

#include "Renderer.h"

namespace Iris {

	/*
	 * TODO: Look at the IMPORTANT NOTE written in Renderer/Core/Device.h about using StagingBuffers in a better more sophisticated way
	 */

	StorageBuffer::StorageBuffer(size_t size, bool deviceLocal)
		: m_Size(size)
	{
		m_LocalData.Allocate(size);

		// Only create the staging buffer and the storage buffer without doing any data uploads since storage buffers most probs dont have preallocated data
		Ref<StorageBuffer> instance = this;
		Renderer::Submit([instance, deviceLocal]() mutable
		{
			VulkanAllocator allocator("StorageBuffer");

			if (deviceLocal)
			{
				VkBufferCreateInfo stagingBufferCI = {
					.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
					.size = instance->m_Size,
					.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					.sharingMode = VK_SHARING_MODE_EXCLUSIVE
				};

				instance->m_StagingBufferAllocation = allocator.AllocateBuffer(&stagingBufferCI, VMA_MEMORY_USAGE_CPU_TO_GPU, &(instance->m_StagingBuffer));
			}

			VkBufferCreateInfo storageBufferCI = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = instance->m_Size,
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE
			};

			instance->m_StorageBufferAllocation = allocator.AllocateBuffer(&storageBufferCI, deviceLocal ? VMA_MEMORY_USAGE_GPU_ONLY : VMA_MEMORY_USAGE_CPU_TO_GPU, &(instance->m_StorageBuffer));

			instance->m_DescriptorInfo.buffer = instance->m_StorageBuffer;
			instance->m_DescriptorInfo.offset = 0;
			instance->m_DescriptorInfo.range = instance->m_Size;
		});
	}

	StorageBuffer::~StorageBuffer()
	{
		Renderer::SubmitReseourceFree([memoryAllocation = m_StorageBufferAllocation, storageBuffer = m_StorageBuffer, stagingBufferAlloc = m_StagingBufferAllocation, stagingBuffer = m_StagingBuffer]()
		{
			VulkanAllocator allocator("StorageBuffer");
			allocator.DestroyBuffer(memoryAllocation, storageBuffer);

			if (stagingBufferAlloc && stagingBuffer)
			{
				allocator.DestroyBuffer(stagingBufferAlloc, stagingBuffer);
			}
		});

		m_LocalData.Release();
	}

	Ref<StorageBuffer> StorageBuffer::Create(size_t size, bool deviceLocal)
	{
		return CreateRef<StorageBuffer>(size, deviceLocal);
	}

	void StorageBuffer::Resize(uint32_t newSize)
	{	
		m_Size = newSize;

		// Only create the staging buffer and the storage buffer without doing any data uploads since storage buffers most probs dont have preallocated data
		Ref<StorageBuffer> instance = this;
		Renderer::Submit([instance]() mutable
		{
			VulkanAllocator allocator("StorageBuffer");

			// If we do not have already a stagingbuffer means that we are not in device local memory
			bool deviceLocal = (instance->m_StagingBufferAllocation != nullptr) && (instance->m_StagingBuffer != nullptr);
			if (deviceLocal)
			{
				VkBufferCreateInfo stagingBufferCI = {
					.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
					.size = instance->m_Size,
					.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					.sharingMode = VK_SHARING_MODE_EXCLUSIVE
				};

				instance->m_StagingBufferAllocation = allocator.AllocateBuffer(&stagingBufferCI, VMA_MEMORY_USAGE_CPU_TO_GPU, &(instance->m_StagingBuffer));
			}

			VkBufferCreateInfo storageBufferCI = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = instance->m_Size,
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE
			};

			instance->m_StorageBufferAllocation = allocator.AllocateBuffer(&storageBufferCI, deviceLocal ? VMA_MEMORY_USAGE_GPU_ONLY : VMA_MEMORY_USAGE_CPU_TO_GPU, &(instance->m_StorageBuffer));

			instance->m_DescriptorInfo.buffer = instance->m_StorageBuffer;
			instance->m_DescriptorInfo.offset = 0;
			instance->m_DescriptorInfo.range = instance->m_Size;
		});
	}

	void StorageBuffer::SetData(const void* data, size_t size, size_t offset)
	{
		IR_ASSERT(size <= m_Size);
		std::memcpy(m_LocalData.Data, reinterpret_cast<const uint8_t*>(data) + offset, size);
		Ref<StorageBuffer> instance = this;
		Renderer::Submit([instance, size, offset]() mutable
		{
			instance->RT_SetData(instance->m_LocalData.Data, size, offset);
		});
	}

	void StorageBuffer::RT_SetData(const void* data, size_t size, size_t offset)
	{
		Ref<VulkanDevice> device = RendererContext::GetCurrentDevice();
		VulkanAllocator allocator("StorageBuffer");

		if (m_StagingBufferAllocation && m_StagingBuffer)
		{
			// mem map to staging buffer, copy mem, then copy staging buffer to storage buffer
			uint8_t* dstData = allocator.MapMemory<uint8_t>(m_StagingBufferAllocation);
			std::memcpy(dstData, reinterpret_cast<const uint8_t*>(data) + offset, size);
			allocator.UnmapMemory(m_StagingBufferAllocation);

			// TODO: Refer to the note in Renderer/Core/Device.h since maybe we could just begin and return a pre-allocated buffer?
			VkCommandBuffer commandBuffer = device->GetCommandBuffer(true);

			VkBufferCopy copyRegion = {
				.srcOffset = offset,
				.dstOffset = offset,
				.size = m_Size
			};

			vkCmdCopyBuffer(commandBuffer, m_StagingBuffer, m_StorageBuffer, 1, &copyRegion);

			// TODO: Refer to the note in Renderer/Core/Device.h since we do not want to do this really...
			device->FlushCommandBuffer(commandBuffer);

			//  NOTE: Here we do not delete the staging buffer in case we want to reuse it...
		}
		else
		{
			// mem map and copy to storage buffer directly
			uint8_t* dstData = allocator.MapMemory<uint8_t>(m_StorageBufferAllocation);
			std::memcpy(dstData, reinterpret_cast<const uint8_t*>(data) + offset, size);
			allocator.UnmapMemory(m_StorageBufferAllocation);
		}
	}

}