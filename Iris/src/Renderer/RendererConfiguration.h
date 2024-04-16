#pragma once

#include <cstdint>

namespace Iris {

	struct RendererConfiguration
	{
		// Default to 3 Frames in flight
		uint32_t FramesInFlight = 3;
	};

}