#pragma once

#include "UniformBuffer.h"

#include <map>

namespace Iris {

	class UniformBufferSet : public RefCountedObject
	{
	public:
		UniformBufferSet(std::size_t size, uint32_t framesInFlight);
		~UniformBufferSet() = default;

		[[nodiscard]] inline static Ref<UniformBufferSet> Create(std::size_t size, uint32_t frame = 0)
		{
			return CreateRef<UniformBufferSet>(size, frame);
		}

		Ref<UniformBuffer> Get();
		Ref<UniformBuffer> RT_Get();

		inline Ref<UniformBuffer> Get(uint32_t frame)
		{
			IR_ASSERT(m_UniformBuffers.find(frame) != m_UniformBuffers.end());
			return m_UniformBuffers.at(frame);
		}

		inline void Set(Ref<UniformBuffer> uniformBuffer, uint32_t frame = 0)
		{
			m_UniformBuffers[frame] = uniformBuffer;
		}

	private:
		std::map<uint32_t, Ref<UniformBuffer>> m_UniformBuffers;

	};

}