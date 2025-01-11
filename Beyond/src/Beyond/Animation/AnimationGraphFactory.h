#pragma once

#include "NodeProcessor.h"

#include "Beyond/Core/Base.h"
#include "Beyond/Core/Identifier.h"
#include "Beyond/Core/Ref.h"
#include "Beyond/Core/UUID.h"

namespace Beyond::AnimationGraph {

	struct AnimationGraph;
	struct Prototype;

	class Factory
	{
		Factory() = delete;
	public:
		[[nodiscard]] static NodeProcessor* Create(Identifier nodeTypeID, UUID nodeID);
		static bool Contains(Identifier nodeTypeID);
	};

	Ref<AnimationGraph> CreateInstance(const Prototype& prototype);

} // namespace Beyond::AnimationGraph
