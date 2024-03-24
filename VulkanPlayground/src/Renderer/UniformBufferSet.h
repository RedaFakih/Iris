#pragma once

/*
 * Abstraction for the uniform buffers since we need to have `frame in flight` number of uniform buffers
 */

#include "UniformBuffer.h"

#include <map>

namespace vkPlayground {

	class UniformBufferSet : public RefCountedObject
	{
	public:
		UniformBufferSet(size_t size, uint32_t framesInFlight);

		~UniformBufferSet() = default;

		[[nodiscard]] inline static Ref<UniformBufferSet> Create(size_t size, uint32_t frame = 0)
		{
			return CreateRef<UniformBufferSet>(size, frame);
		}

		Ref<UniformBuffer> Get();

		inline Ref<UniformBuffer> Get(uint32_t frame)
		{
			VKPG_ASSERT(m_UniformBuffers.find(frame) != m_UniformBuffers.end());
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