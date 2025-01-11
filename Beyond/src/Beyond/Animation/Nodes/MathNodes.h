#pragma once
#include "Beyond/Animation/NodeDescriptor.h"
#include "Beyond/Animation/NodeProcessor.h"
#include "Beyond/Core/UUID.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/log_base.hpp>
#undef GLM_ENABLE_EXPERIMENTAL

namespace Beyond::AnimationGraph {

	template<typename T>
	struct Add : public NodeProcessor
	{
		explicit Add(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value1 = nullptr; //? this indirection is still not ideal
		T* in_Value2 = nullptr;
		T out_Out{ 0 };

		float Process(float) override
		{
			out_Out = (*in_Value1) + (*in_Value2);
			return 0.0f;
		}
	};


	template<typename T>
	struct Subtract : public NodeProcessor
	{
		Subtract(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value1 = nullptr;
		T* in_Value2 = nullptr;
		T out_Out{ 0 };

		float Process(float) override
		{
			out_Out = (*in_Value1) - (*in_Value2);
			return 0.0f;
		}
	};


	template<typename T>
	struct Multiply : public NodeProcessor
	{
		explicit Multiply(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value = nullptr;
		T* in_Multiplier = nullptr;
		T out_Out{ 0 };

		float Process(float) override
		{
			out_Out = (*in_Value) * (*in_Multiplier);
			return 0.0f;
		}
	};


	template<typename T>
	struct Divide : public NodeProcessor
	{
		explicit Divide(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value = nullptr;
		T* in_Divisor = nullptr;
		T out_Out{ 0 };

		float Process(float) override
		{
			if ((*in_Divisor) == T(0))
				out_Out = T(-1);
			else
				out_Out = (*in_Value) / (*in_Divisor);
			return 0.0f;
		}
	};


	struct Modulo : public NodeProcessor
	{
		explicit Modulo(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		int* in_Value = nullptr;
		int* in_Divisor = nullptr;
		int out_Out{ 0 };

		float Process(float) override
		{
			out_Out = (*in_Value) % (*in_Divisor);
			return 0.0f;
		}
	};


	struct Power : public NodeProcessor
	{
		explicit Power(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		float* in_Base = nullptr;
		float* in_Exponent = nullptr;
		float out_Out{ 0.0f };

		virtual float Process(float timestep) override
		{
			out_Out = glm::pow((*in_Base), (*in_Exponent));
			return 0.0;
		}
	};


	struct Log : public NodeProcessor
	{
		explicit Log(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		float* in_Value = nullptr;
		float* in_Base = nullptr;
		float out_Out{ 0.0f };

		float Process(float) override
		{
			out_Out = glm::log((*in_Value), (*in_Base));
			return 0.0f;
		}
	};


	template<typename T>
	struct Min : public NodeProcessor
	{
		explicit Min(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value1 = nullptr;
		T* in_Value2 = nullptr;
		T out_Value{ 0 };

		float Process(float) override
		{
			out_Value = glm::min((*in_Value1), (*in_Value2));
			return 0.0f;
		}
	};


	template<typename T>
	struct Max : public NodeProcessor
	{
		explicit Max(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value1 = nullptr;
		T* in_Value2 = nullptr;
		T out_Value{ 0 };

		float Process(float) override
		{
			out_Value = glm::max((*in_Value1), (*in_Value2));
			return 0.0f;
		}
	};


	template<typename T>
	struct Clamp : public NodeProcessor
	{
		explicit Clamp(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value = nullptr;
		T* in_Min = nullptr;
		T* in_Max = nullptr;
		T out_Value{ 0 };

		float Process(float) override
		{
			out_Value = glm::clamp((*in_Value), (*in_Min), (*in_Max));
			return 0.0f;
		}
	};


	template<typename T>
	struct MapRange : public NodeProcessor
	{
		explicit MapRange(const char* dbgName, UUID id) : NodeProcessor(dbgName, id)
		{
			EndpointUtilities::RegisterEndpoints(this);
		}

		void Init(const Skeleton*) override
		{
			EndpointUtilities::InitializeInputs(this);
		}

		T* in_Value = nullptr;
		T* in_InRangeMin = nullptr;
		T* in_InRangeMax = nullptr;
		T* in_OutRangeMin = nullptr;
		T* in_OutRangeMax = nullptr;
		bool* in_Clamped = nullptr;

		T out_Value{ 0 };

		float Process(float) override
		{
			const bool clamped = (*in_Clamped);
			const T value = clamped ? glm::clamp((*in_Value), (*in_InRangeMin), (*in_InRangeMax)) : (*in_Value);
			const float t = (float)value / ((*in_InRangeMax) - (*in_InRangeMin));

			out_Value = glm::mix((*in_OutRangeMin), (*in_OutRangeMax), t);
			return 0.0f;
		}
	};

} //Beyond::AnimationGraph


DESCRIBE_NODE(Beyond::AnimationGraph::Add<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Add<float>::in_Value1,
		&Beyond::AnimationGraph::Add<float>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Add<float>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Add<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Add<int>::in_Value1,
		&Beyond::AnimationGraph::Add<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Add<int>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Subtract<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Subtract<float>::in_Value1,
		&Beyond::AnimationGraph::Subtract<float>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Subtract<float>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Subtract<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Subtract<int>::in_Value1,
		&Beyond::AnimationGraph::Subtract<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Subtract<int>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Multiply<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Multiply<float>::in_Value,
		&Beyond::AnimationGraph::Multiply<float>::in_Multiplier),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Multiply<float>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Multiply<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Multiply<int>::in_Value,
		&Beyond::AnimationGraph::Multiply<int>::in_Multiplier),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Multiply<int>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Divide<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Divide<float>::in_Value,
		&Beyond::AnimationGraph::Divide<float>::in_Divisor),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Divide<float>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Divide<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Divide<int>::in_Value,
		&Beyond::AnimationGraph::Divide<int>::in_Divisor),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Divide<int>::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Power,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Power::in_Base,
		&Beyond::AnimationGraph::Power::in_Exponent),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Power::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Log,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Log::in_Base,
		&Beyond::AnimationGraph::Log::in_Value),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Log::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Modulo,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Modulo::in_Value,
		&Beyond::AnimationGraph::Modulo::in_Divisor),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Modulo::out_Out)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Min<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Min<float>::in_Value1,
		&Beyond::AnimationGraph::Min<float>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Min<float>::out_Value)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Min<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Min<int>::in_Value1,
		&Beyond::AnimationGraph::Min<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Min<int>::out_Value)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Max<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Max<float>::in_Value1,
		&Beyond::AnimationGraph::Max<float>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Max<float>::out_Value)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Max<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Max<int>::in_Value1,
		&Beyond::AnimationGraph::Max<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Max<int>::out_Value)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Clamp<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Clamp<float>::in_Value,
		&Beyond::AnimationGraph::Clamp<float>::in_Min,
		&Beyond::AnimationGraph::Clamp<float>::in_Max),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Clamp<float>::out_Value)
);

DESCRIBE_NODE(Beyond::AnimationGraph::Clamp<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::Clamp<int>::in_Value,
		&Beyond::AnimationGraph::Clamp<int>::in_Min,
		&Beyond::AnimationGraph::Clamp<int>::in_Max),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::Clamp<int>::out_Value)
);

DESCRIBE_NODE(Beyond::AnimationGraph::MapRange<float>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::MapRange<float>::in_Value,
		&Beyond::AnimationGraph::MapRange<float>::in_InRangeMin,
		&Beyond::AnimationGraph::MapRange<float>::in_InRangeMax,
		&Beyond::AnimationGraph::MapRange<float>::in_OutRangeMin,
		&Beyond::AnimationGraph::MapRange<float>::in_OutRangeMax,
		&Beyond::AnimationGraph::MapRange<float>::in_Clamped),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::MapRange<float>::out_Value)
);

DESCRIBE_NODE(Beyond::AnimationGraph::MapRange<int>,
	NODE_INPUTS(
		&Beyond::AnimationGraph::MapRange<int>::in_Value,
		&Beyond::AnimationGraph::MapRange<int>::in_InRangeMin,
		&Beyond::AnimationGraph::MapRange<int>::in_InRangeMax,
		&Beyond::AnimationGraph::MapRange<int>::in_OutRangeMin,
		&Beyond::AnimationGraph::MapRange<int>::in_OutRangeMax,
		&Beyond::AnimationGraph::MapRange<int>::in_Clamped),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::MapRange<int>::out_Value)
);
