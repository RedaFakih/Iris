#include "IrisPCH.h"
#include "StorageBufferSet.h"

#include "Renderer.h"

namespace Iris {

	StorageBufferSet::StorageBufferSet(bool deviceLocal, uint32_t size, uint32_t framesInFlight)
	{
		if (framesInFlight == 0)
			framesInFlight = Renderer::GetConfig().FramesInFlight;

		for (uint32_t frame = 0; frame < framesInFlight; frame++)
			m_StorageBuffers[frame] = StorageBuffer::Create(size, deviceLocal);
	}

	Ref<StorageBuffer> StorageBufferSet::Get()
	{
		uint32_t frame = Renderer::GetCurrentFrameIndex();
		return Get(frame);
	}

	Ref<StorageBuffer> StorageBufferSet::RT_Get()
	{
		uint32_t frame = Renderer::RT_GetCurrentFrameIndex();
		return Get(frame);
	}

}