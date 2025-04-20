#include "IrisPCH.h"
#include "UniformBufferSet.h"

#include "Renderer.h"

namespace Iris {

	UniformBufferSet::UniformBufferSet(std::size_t size, uint32_t framesInFlight)
	{
		if (framesInFlight == 0)
			framesInFlight = Renderer::GetConfig().FramesInFlight;

		for (uint32_t frame = 0; frame < framesInFlight; frame++)
			m_UniformBuffers[frame] = UniformBuffer::Create(size);
	}

	Ref<UniformBuffer> UniformBufferSet::Get()
	{
		uint32_t frame = Renderer::GetCurrentFrameIndex();
		return Get(frame);
	}

	Ref<UniformBuffer> UniformBufferSet::RT_Get()
	{
		uint32_t frame = Renderer::RT_GetCurrentFrameIndex();
		return Get(frame);
	}

}