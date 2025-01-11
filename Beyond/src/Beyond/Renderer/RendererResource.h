#pragma once

#include "Beyond/Asset/Asset.h"

namespace Beyond {

	using ResourceDescriptorInfo = void*;

	class RendererResource : public Asset
	{
	public:
		virtual ResourceDescriptorInfo GetDescriptorInfo() const = 0;
		virtual uint32_t GetBindlessIndex() const = 0;
		virtual uint32_t GetFlaggedBindlessIndex() const = 0;
	};

}
