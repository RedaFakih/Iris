#pragma once

#include "Core/Base.h"
#include "Core/Buffer.h"
#include "Renderer/Core/VulkanAllocator.h"

namespace Iris {

	class UniformBuffer : public RefCountedObject
	{
	public:
		UniformBuffer(std::size_t size);
		~UniformBuffer();

		[[nodiscard]] static Ref<UniformBuffer> Create(std::size_t size);

		void SetData(const void* data, std::size_t size, std::size_t offset = 0);
		void RT_SetData(const void* data, std::size_t size, std::size_t offset = 0);

		VkDescriptorBufferInfo& GetDescriptorBufferInfo() { return m_DescriptorInfo; }

	private:
		std::size_t m_Size = 0;
		Buffer m_LocalData;

		VkBuffer m_VulkanBuffer = nullptr;
		VkDescriptorBufferInfo m_DescriptorInfo = {};

		VmaAllocation m_MemoryAllocation = nullptr;
	};

}