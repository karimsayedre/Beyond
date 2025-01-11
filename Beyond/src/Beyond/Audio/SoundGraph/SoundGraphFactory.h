#pragma once
#include "NodeProcessor.h"
#include "Beyond/Core/Identifier.h"

namespace Beyond::SoundGraph
{
	class Factory
	{
		Factory() = delete;
	public:
		[[nodiscard]] static NodeProcessor* Create(Identifier nodeTypeID, UUID nodeID);
		static bool Contains(Identifier nodeTypeID);
	};

} // namespace Beyond::SoundGraph
