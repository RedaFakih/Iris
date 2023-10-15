#include "RenderCommandQueue.h"

#include <cstring>

namespace vkPlayground {

	RenderCommandQueue::RenderCommandQueue()
	{
		m_CommandBuffer = new uint8_t[10 * 1024 * 1024]; // 10mb buffer
		m_CommandBufferPtr = m_CommandBuffer;
		memset(m_CommandBuffer, 0, 10 * 1024 * 1024);
	}

	RenderCommandQueue::~RenderCommandQueue()
	{
		delete[] m_CommandBuffer;
	}

	void* RenderCommandQueue::Allocate(RenderCommandFn fn, uint32_t size)
	{
		// TODO: Alignment
		// 
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
		
		// We store the function pointer in the buffer then advance the pointer so that next write will not overwrite the function pointer
		*reinterpret_cast<RenderCommandFn*>(m_CommandBufferPtr) = fn;
		m_CommandBufferPtr += sizeof(RenderCommandFn);
		
		// We store the size in the buffer then advance the pointer so that next write will not overwrite the size in the buffer
		*reinterpret_cast<uint32_t*>(m_CommandBufferPtr) = size;
		m_CommandBufferPtr += sizeof(uint32_t);
		
		// We return the memory after both the function and its size have been stored
		void* memory = m_CommandBufferPtr;
		m_CommandBufferPtr += size;
		
		m_CommandCount++;
		return memory;
	}

	void RenderCommandQueue::Execute()
	{
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
		m_CommandCount = 0;
	}

}