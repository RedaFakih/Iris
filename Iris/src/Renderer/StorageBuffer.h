#pragma once

#include "Core/Buffer.h"
#include "Renderer/Core/VulkanAllocator.h"

namespace Iris {

	class StorageBuffer : public RefCountedObject
	{
	public:
		StorageBuffer(size_t size, bool deviceLocal = true);
		~StorageBuffer();

		[[nodiscard]] static Ref<StorageBuffer> Create(size_t size, bool deviceLocal = true);

		void Resize(uint32_t newSize);
		void SetData(const void* data, size_t size, size_t offset = 0);
		void RT_SetData(const void* data, size_t size, size_t offset = 0);

		VkDescriptorBufferInfo& GetDescriptorBufferInfo() { return m_DescriptorInfo; }

		bool IsDeviceLocal() const { return m_StagingBuffer != nullptr; }

	private:
		size_t m_Size = 0;
		Buffer m_LocalData;

		VkBuffer m_StorageBuffer = nullptr;
		VkBuffer m_StagingBuffer = nullptr; // We store the pointer for the staging buffer so that we dont have to create it and destroy on each invalidation
		VkDescriptorBufferInfo m_DescriptorInfo = {};

		VmaAllocation m_StorageBufferAllocation = nullptr;
		VmaAllocation m_StagingBufferAllocation = nullptr;
	};

}