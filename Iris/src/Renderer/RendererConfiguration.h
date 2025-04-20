#pragma once

#include <cstdint>

namespace Iris {

	struct RendererConfiguration
	{
		// Default to 3 Frames in flight
		uint32_t FramesInFlight = 3;

		// All the following members are serialized

		// This disables creating filtered environment maps (disable both radiance and irradiance maps)
		bool ComputeEnvironmentMaps = true;

		uint32_t EnvironmentMapResolution = 1024;
		uint32_t IrradianceMapComputeSamples = 512;

		uint32_t MaxNumberOfPointLights = 100;
		uint32_t MaxNumberOfSpotLights = 100;
	};

	struct RendererConfigurationSerialzier
	{
		static void Init();

		static void Serialize(const RendererConfiguration& config);
		static void Deserialize(RendererConfiguration& config);
	};

}