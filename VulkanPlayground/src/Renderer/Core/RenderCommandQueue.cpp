#include "RenderCommandQueue.h"

#include <cstring>

namespace vkPlayground {

	RenderCommandQueue::RenderCommandQueue()
	{
		m_Capacity = 1024; // 1KB
		m_CommandBuffer = new uint8_t[m_Capacity]; // Starting buffer of 1KB capacity
		m_CommandBufferPtr = m_CommandBuffer;
		std::memset(m_CommandBuffer, 0, m_Capacity);
	}

	RenderCommandQueue::~RenderCommandQueue()
	{
		delete[] m_CommandBuffer;
		m_CommandBuffer = nullptr;
		m_CommandBufferPtr = nullptr;
	}

	void* RenderCommandQueue::Allocate(RenderCommandFn fn, uint32_t size)
	{
		// IMPORTANT NOTE:
		// In memory the layout of the buffer is:
		// |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
		// | fn | size | FuncT using placement new | fn | size | FuncT using placemet new |  ....  |
		// |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
		// 
		// fn here would be:
		//		auto renderCmd = [](void* ptr) { auto pFunc = (FuncT*)ptr; (*pFunc)(); pFunc->~FuncT(); };
		// and the `void* ptr` argument is the one that we store using the placement new which would contain all our vulkan commands
		// 
		// The size is stored in the buffer so that when we want to execute the commands one by one we can know how much to advance in
		// order to advance the pointer to the next command correctly

		// Should also handle the case where we allocate but there is still no space to fit the render command func but that is rare and
		// if that happens then we messed up really badly to get there

		// Pre-compute the used bytes so that we do not write outside of the commandbuffer when writing the data
		m_UsedBytes += sizeof(RenderCommandFn) + sizeof(uint32_t) + size;
		// Double on used bytes since at this point the capacity is lower...
		if (m_UsedBytes >= m_Capacity)
			ReAlloc(m_UsedBytes * 2, static_cast<std::size_t>(m_CommandBufferPtr - m_CommandBuffer));

		// We store the function pointer in the buffer then advance the pointer so that next write will not overwrite the function pointer
		*reinterpret_cast<RenderCommandFn*>(m_CommandBufferPtr) = fn;
		m_CommandBufferPtr += sizeof(RenderCommandFn);
		
		// We store the size in the buffer then advance the pointer so that next write will not overwrite the size in the buffer
		*reinterpret_cast<uint32_t*>(m_CommandBufferPtr) = size;
		m_CommandBufferPtr += sizeof(uint32_t);
		
		// We return the memory after both the function and its size have been stored
		void* memory = m_CommandBufferPtr;
		m_CommandBufferPtr += size;
		m_UsedBytes = static_cast<size_t>(m_CommandBufferPtr - m_CommandBuffer); // Just for sanity checks (value should not change after this)
		
		m_CommandCount++;
		return memory;
	}

	void RenderCommandQueue::Execute()
	{
		// Start from the base of the command buffer
		uint8_t* buffer = m_CommandBuffer;

		for (uint32_t i = 0; i < m_CommandCount; i++)
		{
			RenderCommandFn function = *reinterpret_cast<RenderCommandFn*>(buffer);
			buffer += sizeof(RenderCommandFn);

			uint32_t size = *reinterpret_cast<uint32_t*>(buffer);
			buffer += sizeof(uint32_t);

			// NOTE:
			// `buffer` here would be pointing to the beginning of the function containing all the vulkan commands and `function` would be the
			// lambda that calls the function pointed to by the buffer
			function(buffer);

			// Advance buffer to the next RenderCommandFn (In other words, advance the buffer past the function containing all vulkan commands)
			buffer += size;
		}

		m_CommandBufferPtr = m_CommandBuffer; // Reset the Ptr to the beginning of the CommandBuffer
		m_UsedBytes = 0; // Reset the used bytes after execution for debugging purposes
		m_CommandCount = 0;
	}

	void RenderCommandQueue::ReAlloc(std::size_t newCapacity, std::size_t ptrPosition)
	{
		uint8_t* newCommandBuffer = new uint8_t[newCapacity];
		std::memcpy(newCommandBuffer, m_CommandBuffer, m_Capacity);

		delete[] m_CommandBuffer;
		m_CommandBuffer = newCommandBuffer;
		m_CommandBufferPtr = m_CommandBuffer + ptrPosition; // Restore the pointer to the same position as it was on the past buffer

		newCommandBuffer = nullptr;
		m_Capacity = newCapacity;
	}

}