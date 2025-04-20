#pragma once

#include "StorageBuffer.h"

#include <map>

namespace Iris {

	// TODO: Resize
	class StorageBufferSet : public RefCountedObject
	{
	public:
		StorageBufferSet(bool deviceLocal, uint32_t size, uint32_t framesInFlight);
		~StorageBufferSet() = default;

		[[nodiscard]] inline static Ref<StorageBufferSet> Create(bool deviceLocal, uint32_t size, uint32_t framesInFlight = 0)
		{
			return CreateRef<StorageBufferSet>(deviceLocal, size, framesInFlight);
		}

		Ref<StorageBuffer> Get();
		Ref<StorageBuffer> RT_Get();
		void Resize(uint32_t newSize);

		inline Ref<StorageBuffer> Get(uint32_t frameInFlight)
		{
			IR_ASSERT(m_StorageBuffers.contains(frameInFlight));
			return m_StorageBuffers.at(frameInFlight);
		}

		inline void Set(Ref<StorageBuffer> storageBuffer, uint32_t frameInFlight = 0)
		{
			m_StorageBuffers[frameInFlight] = storageBuffer;
		}

	private:
		std::map<uint32_t, Ref<StorageBuffer>> m_StorageBuffers;

	};

}