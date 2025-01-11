#pragma once

#include <string>

namespace Beyond {

	struct RendererCapabilities
	{
		eastl::string Vendor;
		eastl::string Device;
		eastl::string Version;

		int MaxSamples = 0;
		float MaxAnisotropy = 0.0f;
		int MaxTextureUnits = 0;
	};


}
