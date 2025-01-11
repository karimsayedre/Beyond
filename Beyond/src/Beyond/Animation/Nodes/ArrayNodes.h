#pragma once
#include "Beyond/Animation/NodeDescriptor.h"
#include "Beyond/Animation/NodeProcessor.h"

#define DECLARE_ID(name) static constexpr Identifier name{ #name }

namespace Beyond::AnimationGraph {

	// Sometimes we need to reuse NodeProcessor types for different nodes,
	// so that they have different names and other editor UI properties
	// but the same NodeProcessor type in the backend.
	// In such case we declare alias ID here and define it in the AnimationGraphNodes.cpp,
	// adding and extra entry to the registry of the same NodeProcessor type,
	// but with a different name.
	// Actual NodeProcessor type for the alias is assigned in AnimationGraphFactory.cpp
	//! Aliases must already be "User Friendly Type Name" in format: "Get Random (Float)" instead of "GetRandom<float>"
	namespace Alias {
		static constexpr auto GetAnimation = "Get (Animation)";
	}

	template<typename T>
	struct Get : public NodeProcessor
	{
		struct IDs
		{
			DECLARE_ID(Array);
		private:
			IDs() = delete;
		};

		explicit Get(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		T* in_Array = nullptr;
		int32_t* in_Index = nullptr;

		T out_Element{ 0 };

	public:
		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);

			const uint32_t arraySize = InValue(IDs::Array).size();
			const auto index = (uint32_t)(*in_Index);

			const auto& element = in_Array[(index >= arraySize) ? (index % arraySize) : index];
			out_Element = element;
		}

		float Process(float) override
		{
			const uint32_t arraySize = InValue(IDs::Array).size();
			const auto index = (uint32_t)(*in_Index);

			out_Element = in_Array[(index >= arraySize) ? (index % arraySize) : index];
			return 0.0f;
		}
	};

} // namespace Beyond::AnimationGraph


DESCRIBE_NODE(Beyond::AnimationGraph::Get<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Get<float>::in_Array,
		&Beyond::AnimationGraph::Get<float>::in_Index),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Get<float>::out_Element)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Get<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Get<int>::in_Array,
		&Beyond::AnimationGraph::Get<int>::in_Index),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Get<int>::out_Element)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Get<int64_t>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Get<int64_t>::in_Array,
		&Beyond::AnimationGraph::Get<int64_t>::in_Index),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Get<int64_t>::out_Element)
);


#undef DECLARE_ID
