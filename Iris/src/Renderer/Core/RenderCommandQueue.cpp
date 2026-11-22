#include "IrisPCH.h"
#include "RenderCommandQueue.h"

namespace Iris {

	RenderCommandQueue::~RenderCommandQueue()
	{
		for (Chunk& c : m_Chunks)
			::operator delete(c.Data, std::align_val_t{ c_Alignment });
	}

	void* RenderCommandQueue::Allocate(RenderCommandFn fn, uint32_t size)
	{
		const std::size_t payload = AlignUp(size);
		const std::size_t needed = sizeof(CommandHeader) + payload;

		// Chunked linear allocator, allocates a new chunk when an already in use chunk has exhausted its capacity for new render commands.
		// This has the advantage that after the the allocator has enough chunks, subsequent frames may have to do allocate zero new chunks and just reuse what is available.

		if (m_Chunks.empty() || m_Chunks[m_Current].Used + needed > m_Chunks[m_Current].Capacity)
		{
			if (!m_Chunks.empty())
				++m_Current;

			if (m_Current >= m_Chunks.size() || m_Chunks[m_Current].Capacity < needed)
			{
				const std::size_t capacity = std::max(c_DefaultChunkSize, needed);
				Chunk chunk = {
					.Data = static_cast<uint8_t*>(::operator new(capacity, std::align_val_t{ c_Alignment })),
					.Capacity = capacity
				};

				m_Chunks.insert(m_Chunks.begin() + m_Current, chunk);
			}
		}

		Chunk& chunk = m_Chunks[m_Current];
		CommandHeader* header = new(chunk.Data + chunk.Used) CommandHeader{ .Fn = fn, .PayloadSize = static_cast<uint32_t>(payload) };
		void* memory = reinterpret_cast<uint8_t*>(header) + sizeof(CommandHeader);
		chunk.Used += needed;
		
		return memory;
	}

	void RenderCommandQueue::Execute()
	{
		if (m_Chunks.empty())
			return;

		for (std::size_t i = 0; i <= m_Current; i++)
		{
			Chunk& chunk = m_Chunks[i];
			std::size_t offset = 0;
			while (offset < chunk.Used)
			{
				CommandHeader* header = reinterpret_cast<CommandHeader*>(chunk.Data + offset);
				void* payload = reinterpret_cast<uint8_t*>(header) + sizeof(CommandHeader);
				
				header->Fn(payload);

				offset += sizeof(CommandHeader) + header->PayloadSize;
			}

			// Reset per chunk for next frame after all command have been executed
			chunk.Used = 0;
		}

		// Reset to chunk 0 after everything was executed to reuse next frame
		m_Current = 0;
	}

}