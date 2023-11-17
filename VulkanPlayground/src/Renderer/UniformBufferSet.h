#pragma once

/*
 * Abstraction for the uniform buffers since we need to have `frame in flight` number of uniform buffers
 */

#include "Renderer.h"
#include "UniformBuffer.h"

#include <map>

namespace vkPlayground {

	class UniformBufferSet : public RefCountedObject
	{
	public:
		UniformBufferSet(size_t size, uint32_t framesInFlight)
		{
			if (framesInFlight == 0)
				framesInFlight = Renderer::GetConfig().FramesInFlight;
		
			for (uint32_t frame = 0; frame < framesInFlight; frame++)
				m_UniformBuffers[frame] = UniformBuffer::Create(size);
		}

		~UniformBufferSet()
		{
		}

		[[nodiscard]] inline static Ref<UniformBufferSet> Create(size_t size, uint32_t frame = 0)
		{
			return CreateRef<UniformBufferSet>(size, frame);
		}

		inline Ref<UniformBuffer> Get()
		{
			uint32_t frame = Renderer::GetCurrentFrameIndex();
			return Get(frame);
		}

		inline Ref<UniformBuffer> Get(uint32_t frame)
		{
			PG_ASSERT(m_UniformBuffers.find(frame) != m_UniformBuffers.end(), "");
			return m_UniformBuffers.at(frame);
		}

		inline void Set(Ref<UniformBuffer> uniformBuffer, uint32_t frame = 0)
		{
			m_UniformBuffers[frame] = uniformBuffer;
		}

	private:
		// NOTE: Maybe could be just a vector of pairs? instead of being a map? Since we won't have that much UBOs so searching for the index
		// will not require alot of computation cycles since n will be relatively small
		std::map<uint32_t, Ref<UniformBuffer>> m_UniformBuffers;
	};

}