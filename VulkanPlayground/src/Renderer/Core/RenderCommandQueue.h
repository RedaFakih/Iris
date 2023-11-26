#pragma once

#include <cstdint>
#include <cstddef>

namespace vkPlayground {

	class RenderCommandQueue
	{
	public:
		using RenderCommandFn = void(*)(void*);

		RenderCommandQueue();
		~RenderCommandQueue();

		void* Allocate(RenderCommandFn fn, uint32_t size);
		void Execute();

	private:
		void ReAlloc(std::size_t newCapacity, std::size_t ptrPosition);

	private:
		uint8_t* m_CommandBuffer = nullptr; // Points to the beginning of the buffer
		uint8_t* m_CommandBufferPtr = nullptr; // Moves along the buffer (only a ref to it and does not own it)
		std::size_t m_CommandCount = 0;
		std::size_t m_Capacity = 0;
		std::size_t m_UsedBytes = 0;
	};

}