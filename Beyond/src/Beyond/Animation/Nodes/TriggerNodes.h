#pragma once
#include "Beyond/Animation/NodeDescriptor.h"
#include "Beyond/Animation/NodeProcessor.h"

#define DECLARE_ID(name) static constexpr Identifier name{ #name }

namespace Beyond::AnimationGraph {

	struct BoolTrigger : public NodeProcessor
	{
		struct IDs
		{
			DECLARE_ID(False);
			DECLARE_ID(True);
		private:
			IDs() = delete;
		};

		explicit BoolTrigger(const char* dbgName, UUID id);

		bool* in_Value = &DefaultValue;

		// Runtime default for the above inputs.  Editor defaults are set to the same values (see AnimationGraphNodes.cpp)
		// Individual graphs can override these defaults, in which case the values are saved in this->DefaultValuePlugs
		inline static bool DefaultValue = false;

		OutputEvent out_OnTrue;
		OutputEvent out_OnFalse;

	public:
		void Init(const Skeleton*) override;
		float Process(float) override;
	};

} // namespace Beyond::AnimationGraph


DESCRIBE_NODE(Beyond::AnimationGraph::BoolTrigger,
	NODE_INPUTS(
		&Beyond::AnimationGraph::BoolTrigger::in_Value),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::BoolTrigger::out_OnTrue,
		&Beyond::AnimationGraph::BoolTrigger::out_OnFalse)
);


#undef DECLARE_ID
