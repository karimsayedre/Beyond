#include "pch.h"

#include "RendererConfig.h"
namespace Beyond
{
	RendererConfig::RendererConfig()
		: FramesInFlight(3), ComputeEnvironmentMaps(true), EnvironmentMapResolution(1024), IrradianceMapComputeSamples(512)
	{

	}
}
