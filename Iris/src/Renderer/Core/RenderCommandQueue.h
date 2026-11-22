#pragma once

#include <cstddef>
#include <cstdint>

namespace Iris {

	class RenderCommandQueue
	{
	public:
		using RenderCommandFn = void(*)(void*);

		RenderCommandQueue() = default;
		~RenderCommandQueue();

		RenderCommandQueue(const RenderCommandQueue&) = delete;
		RenderCommandQueue& operator=(const RenderCommandQueue&) = delete;

		RenderCommandQueue(RenderCommandQueue&& other) noexcept { Swap(other); }
		RenderCommandQueue& operator=(RenderCommandQueue&& other) noexcept { Swap(other); return *this; }

		void* Allocate(RenderCommandFn fn, uint32_t size);
		void Execute();

		void Swap(RenderCommandQueue& other) noexcept
		{
			m_Chunks.swap(other.m_Chunks);
			std::swap(m_Current, other.m_Current);
		}

		inline bool IsEmpty() const { return m_Chunks.empty() || (m_Current == 0 && m_Chunks[0].Used == 0); }

	private:
		static constexpr std::size_t AlignUp(std::size_t v) { return (v + c_Alignment - 1) & ~(c_Alignment - 1); }

	private:
		static constexpr std::size_t c_Alignment = 16;
		static constexpr std::size_t c_DefaultChunkSize = 1 << 20; // ~1 MB

		// 16 byte aligned
		struct alignas(c_Alignment) CommandHeader
		{
			RenderCommandFn Fn;
			uint32_t PayloadSize;
		};

		// 16 byte aligned
		struct Chunk
		{
			uint8_t* Data = nullptr;
			std::size_t Capacity = 0;
			std::size_t Used = 0;
		};

		std::vector<Chunk> m_Chunks;
		std::size_t m_Current = 0;

	public:
		static constexpr std::size_t c_MaxAlignment = c_Alignment;

	};

}