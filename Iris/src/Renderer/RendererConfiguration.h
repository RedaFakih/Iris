#pragma once

#include <cstdint>

namespace Iris {

	struct RendererConfiguration
	{
		// Default to 3 Frames in flight
		uint32_t FramesInFlight = 3;

		// This disables creating filtered environment maps (disable both radiance and irradiance maps)
		bool ComputeEnvironmentMaps = true;

		uint32_t EnvironmentMapResolution = 1024;
		uint32_t IrradianceMapComputeSamples = 512;
	};

}