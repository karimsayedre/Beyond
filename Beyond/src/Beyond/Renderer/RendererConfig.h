#pragma once

#include <string>

namespace Beyond {

	struct RendererConfig
	{
		// Note: Faster to change cpp file and compile than to
		// recompile all files that include this header
		RendererConfig(); // Default values
		uint32_t FramesInFlight;

		bool ComputeEnvironmentMaps;

		// Tiering settings
		uint32_t EnvironmentMapResolution;
		uint32_t IrradianceMapComputeSamples;

		std::string ShaderPackPath;
	};

}
