#pragma once
#include "Beyond/Animation/NodeDescriptor.h"
#include "Beyond/Animation/NodeProcessor.h"
#include "Beyond/Core/UUID.h"

namespace Beyond::AnimationGraph {

	template<typename T>
	struct CheckEqual : public NodeProcessor
	{
		explicit CheckEqual(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value1 = nullptr; //? this indirection is still not ideal
		T* in_Value2 = nullptr;
		bool out_Out{ 0 };

		float Process(float timestep) override
		{
			out_Out = (*in_Value1 == *in_Value2);
			return timestep;
		}
	};


	template<typename T>
	struct CheckNotEqual : public NodeProcessor
	{
		explicit CheckNotEqual(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value1 = nullptr; //? this indirection is still not ideal
		T* in_Value2 = nullptr;
		bool out_Out{ 0 };

		float Process(float timestep) override
		{
			out_Out = (*in_Value1 != *in_Value2);
			return timestep;
		}
	};


	template<typename T>
	struct CheckLess : public NodeProcessor
	{
		explicit CheckLess(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value1 = nullptr; //? this indirection is still not ideal
		T* in_Value2 = nullptr;
		bool out_Out{ 0 };

		float Process(float timestep) override
		{
			out_Out = (*in_Value1 < *in_Value2);
			return timestep;
		}
	};


	template<typename T>
	struct CheckLessEqual : public NodeProcessor
	{
		explicit CheckLessEqual(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value1 = nullptr; //? this indirection is still not ideal
		T* in_Value2 = nullptr;
		bool out_Out{ 0 };

		float Process(float timestep) override
		{
			out_Out = (*in_Value1 <= *in_Value2);
			return timestep;
		}
	};


	template<typename T>
	struct CheckGreater : public NodeProcessor
	{
		explicit CheckGreater(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value1 = nullptr; //? this indirection is still not ideal
		T* in_Value2 = nullptr;
		bool out_Out{ 0 };

		float Process(float timestep) override
		{
			out_Out = (*in_Value1 > *in_Value2);
			return timestep;
		}
	};


	template<typename T>
	struct CheckGreaterEqual : public NodeProcessor
	{
		explicit CheckGreaterEqual(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value1 = nullptr; //? this indirection is still not ideal
		T* in_Value2 = nullptr;
		bool out_Out{ 0 };

		float Process(float timestep) override
		{
			out_Out = (*in_Value1 >= *in_Value2);
			return timestep;
		}
	};


	struct And : public NodeProcessor
	{
		explicit And(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		bool* in_Value1 = nullptr; //? this indirection is still not ideal
		bool* in_Value2 = nullptr;
		bool out_Out{ 0 };

		float Process(float timestep) override
		{
			out_Out = *in_Value1 && *in_Value2;
			return timestep;
		}
	};


	struct Or : public NodeProcessor
	{
		explicit Or(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		bool* in_Value1 = nullptr; //? this indirection is still not ideal
		bool* in_Value2 = nullptr;
		bool out_Out{ 0 };

		float Process(float timestep) override
		{
			out_Out = *in_Value1 || *in_Value2;
			return timestep;
		}
	};


	struct Not : public NodeProcessor
	{
		explicit Not(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		bool* in_Value = nullptr; //? this indirection is still not ideal
		bool out_Out{ 0 };

		float Process(float timestep) override
		{
			out_Out = !*in_Value;
			return timestep;
		}
	};

} //Beyond::AnimationGraph


DESCRIBE_NODE(Beyond::AnimationGraph::CheckEqual<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::CheckEqual<int>::in_Value1,
		&Beyond::AnimationGraph::CheckEqual<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::CheckEqual<int>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::CheckNotEqual<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::CheckNotEqual<int>::in_Value1,
		&Beyond::AnimationGraph::CheckNotEqual<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::CheckNotEqual<int>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::CheckLess<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::CheckLess<int>::in_Value1,
		&Beyond::AnimationGraph::CheckLess<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::CheckLess<int>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::CheckLessEqual<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::CheckLessEqual<int>::in_Value1,
		&Beyond::AnimationGraph::CheckLessEqual<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::CheckLessEqual<int>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::CheckGreater<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::CheckGreater<int>::in_Value1,
		&Beyond::AnimationGraph::CheckGreater<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::CheckGreater<int>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::CheckGreaterEqual<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::CheckGreaterEqual<int>::in_Value1,
		&Beyond::AnimationGraph::CheckGreaterEqual<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::CheckGreaterEqual<int>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::CheckEqual<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::CheckEqual<float>::in_Value1,
		&Beyond::AnimationGraph::CheckEqual<float>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::CheckEqual<float>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::CheckNotEqual<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::CheckNotEqual<float>::in_Value1,
		&Beyond::AnimationGraph::CheckNotEqual<float>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::CheckNotEqual<float>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::CheckLess<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::CheckLess<float>::in_Value1,
		&Beyond::AnimationGraph::CheckLess<float>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::CheckLess<float>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::CheckLessEqual<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::CheckLessEqual<float>::in_Value1,
		&Beyond::AnimationGraph::CheckLessEqual<float>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::CheckLessEqual<float>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::CheckGreater<float>,
		NODE_INPUTS(
			&Beyond::AnimationGraph::CheckGreater<float>::in_Value1,
			&Beyond::AnimationGraph::CheckGreater<float>::in_Value2),
		NODE_OUTPUTS(
			&Beyond::AnimationGraph::CheckGreater<float>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::CheckGreaterEqual<float>,
		NODE_INPUTS(
			&Beyond::AnimationGraph::CheckGreaterEqual<float>::in_Value1,
			&Beyond::AnimationGraph::CheckGreaterEqual<float>::in_Value2),
		NODE_OUTPUTS(
			&Beyond::AnimationGraph::CheckGreaterEqual<float>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::And,
	NODE_INPUTS(
		&Beyond::AnimationGraph::And::in_Value1,
		&Beyond::AnimationGraph::And::in_Value2),
		NODE_OUTPUTS(
			&Beyond::AnimationGraph::And::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Or,
		NODE_INPUTS(
			&Beyond::AnimationGraph::Or::in_Value1,
			&Beyond::AnimationGraph::Or::in_Value2),
			NODE_OUTPUTS(
				&Beyond::AnimationGraph::Or::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Not,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Not::in_Value),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Not::out_Out)
);
