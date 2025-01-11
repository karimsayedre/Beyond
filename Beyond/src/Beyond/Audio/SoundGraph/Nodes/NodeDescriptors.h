#pragma once

#include "Beyond/Audio/SoundGraph/NodeProcessor.h"

#include "NodeTypes.h"
#include "Beyond/Reflection/TypeDescriptor.h"

// Sometimes we need to reuse NodeProcessor types for different nodes,
// so that they have different names and other editor UI properties
// but the same NodeProcessor type in the backend.
// In such case we declare alias ID here and define it in the SoundGraphNodes.cpp,
// adding and extra entry to the registry of the same NodeProcessor type,
// but with a differen name.
// Actual NodeProcessor type for the alias is assigned in SoundgGraphFactory.cpp
//! Name aliases must already be "User Friendly Type Name" in format: "Get Random (Float)" instead of "GetRandom<float>"
namespace Beyond::SoundGraph::NameAliases
{
	static constexpr auto AddAudio			= "Add (Audio)"
						, AddFloatAudio		= "Add (Float to Audio)"
						, MultAudioFloat	= "Multiply (Audio by Float)"
						, MultAudio			= "Multiply (Audio)"
						, SubtractAudio		= "Subtract (Audio)"
						, MinAudio			= "Min (Audio)"
						, MaxAudio			= "Max (Audio)"
						, ClampAudio		= "Clamp (Audio)"
						, MapRangeAudio		= "Map Range (Audio)"
						, GetWave			= "Get (Wave)"
						, GetRandomWave		= "Get Random (Wave)"
						, ADEnvelope		= "AD Envelope"
						, BPMToSeconds		= "BPM to Seconds";

#undef NAME_ALIAS
}


// TODO: somehow add value overrides here?
//		Alternativelly I could create a function that takes NodeProcessor type and returns only named overrides and verifies member names

namespace Beyond::SoundGraph {
	struct TagInputs {};
	struct TagOutputs {};
	template<typename T> struct NodeDescription;

	template<typename T>
	using DescribedNode = Type::is_specialized<NodeDescription<std::remove_cvref_t<T>>>;
}

//! Example
#if 0
	DESCRIBED_TAGGED(Beyond::SoundGraph::Add<float>, Beyond::Nodes::TagInputs,
		&Beyond::SoundGraph::Add<float>::in_Value1,
		&Beyond::SoundGraph::Add<float>::in_Value2);

	DESCRIBED_TAGGED(Beyond::SoundGraph::Add<float>, Beyond::Nodes::TagOutputs,
		&Beyond::SoundGraph::Add<float>::out_Out);

	template<> struct Beyond::Nodes::NodeDescription<Beyond::SoundGraph::Add<float>>
	{
		using Inputs = Beyond::Type::Description<Beyond::SoundGraph::Add<float>, Beyond::Nodes::TagInputs>;
		using Outputs = Beyond::Type::Description<Beyond::SoundGraph::Add<float>, Beyond::Nodes::TagOutputs>;
	};
#endif

#ifndef NODE_INPUTS
#define NODE_INPUTS(...) __VA_ARGS__
#endif // !NODE_INPUTS

#ifndef NODE_OUTPUTS
#define NODE_OUTPUTS(...) __VA_ARGS__
#endif // !NODE_OUTPUTS

// TODO: type and name overrides
// TODO: node with no inputs / outputs
#ifndef DESCRIBE_NODE
	#define DESCRIBE_NODE(NodeType, InputList, OutputList)									\
	DESCRIBED_TAGGED(NodeType, Beyond::SoundGraph::TagInputs, InputList)						\
	DESCRIBED_TAGGED(NodeType, Beyond::SoundGraph::TagOutputs, OutputList)					\
																							\
	template<> struct Beyond::SoundGraph::NodeDescription<NodeType>							\
	{																						\
		using Inputs = Beyond::Type::Description<NodeType, Beyond::SoundGraph::TagInputs>;	\
		using Outputs = Beyond::Type::Description<NodeType, Beyond::SoundGraph::TagOutputs>;	\
	};		
#endif // !DESCRIBE_NODE

DESCRIBE_NODE(Beyond::SoundGraph::Add<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Add<float>::in_Value1,
		&Beyond::SoundGraph::Add<float>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Add<float>::out_Out)
);

DESCRIBE_NODE(Beyond::SoundGraph::Add<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Add<int>::in_Value1,
		&Beyond::SoundGraph::Add<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Add<int>::out_Out)
);

DESCRIBE_NODE(Beyond::SoundGraph::Subtract<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Subtract<float>::in_Value1,
		&Beyond::SoundGraph::Subtract<float>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Subtract<float>::out_Out)
);

DESCRIBE_NODE(Beyond::SoundGraph::Subtract<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Subtract<int>::in_Value1,
		&Beyond::SoundGraph::Subtract<int>::in_Value2),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Subtract<int>::out_Out)
);

DESCRIBE_NODE(Beyond::SoundGraph::Multiply<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Multiply<float>::in_Value,
		&Beyond::SoundGraph::Multiply<float>::in_Multiplier),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Multiply<float>::out_Out)
);

DESCRIBE_NODE(Beyond::SoundGraph::Multiply<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Multiply<int>::in_Value,
		&Beyond::SoundGraph::Multiply<int>::in_Multiplier),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Multiply<int>::out_Out)
);

DESCRIBE_NODE(Beyond::SoundGraph::Divide<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Divide<float>::in_Value,
		&Beyond::SoundGraph::Divide<float>::in_Denominator),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Divide<float>::out_Out)
);

DESCRIBE_NODE(Beyond::SoundGraph::Divide<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Divide<int>::in_Value,
		&Beyond::SoundGraph::Divide<int>::in_Denominator),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Divide<int>::out_Out)
);

DESCRIBE_NODE(Beyond::SoundGraph::Power,
	NODE_INPUTS(
		&Beyond::SoundGraph::Power::in_Base,
		&Beyond::SoundGraph::Power::in_Exponent),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Power::out_Out)
);

DESCRIBE_NODE(Beyond::SoundGraph::Log,
	NODE_INPUTS(
		&Beyond::SoundGraph::Log::in_Base,
		&Beyond::SoundGraph::Log::in_Value),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Log::out_Out)
);

DESCRIBE_NODE(Beyond::SoundGraph::LinearToLogFrequency,
	NODE_INPUTS(
		&Beyond::SoundGraph::LinearToLogFrequency::in_Value,
		&Beyond::SoundGraph::LinearToLogFrequency::in_Min,
		&Beyond::SoundGraph::LinearToLogFrequency::in_Max,
		&Beyond::SoundGraph::LinearToLogFrequency::in_MinFrequency,
		&Beyond::SoundGraph::LinearToLogFrequency::in_MaxFrequency),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::LinearToLogFrequency::out_Frequency)
);

DESCRIBE_NODE(Beyond::SoundGraph::FrequencyLogToLinear,
	NODE_INPUTS(
		&Beyond::SoundGraph::FrequencyLogToLinear::in_Frequency,
		&Beyond::SoundGraph::FrequencyLogToLinear::in_MinFrequency,
		&Beyond::SoundGraph::FrequencyLogToLinear::in_MaxFrequency,
		&Beyond::SoundGraph::FrequencyLogToLinear::in_Min,
		&Beyond::SoundGraph::FrequencyLogToLinear::in_Max),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::FrequencyLogToLinear::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::Modulo,
	NODE_INPUTS(
		&Beyond::SoundGraph::Modulo::in_Value,
		&Beyond::SoundGraph::Modulo::in_Modulo),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Modulo::out_Out)
);

DESCRIBE_NODE(Beyond::SoundGraph::Min<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Min<float>::in_A,
		&Beyond::SoundGraph::Min<float>::in_B),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Min<float>::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::Min<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Min<int>::in_A,
		&Beyond::SoundGraph::Min<int>::in_B),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Min<int>::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::Max<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Max<float>::in_A,
		&Beyond::SoundGraph::Max<float>::in_B),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Max<float>::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::Max<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Max<int>::in_A,
		&Beyond::SoundGraph::Max<int>::in_B),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Max<int>::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::Clamp<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Clamp<float>::in_In,
		&Beyond::SoundGraph::Clamp<float>::in_Min,
		&Beyond::SoundGraph::Clamp<float>::in_Max),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Clamp<float>::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::Clamp<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Clamp<int>::in_In,
		&Beyond::SoundGraph::Clamp<int>::in_Min,
		&Beyond::SoundGraph::Clamp<int>::in_Max),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Clamp<int>::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::MapRange<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::MapRange<float>::in_In,
		&Beyond::SoundGraph::MapRange<float>::in_InRangeA,
		&Beyond::SoundGraph::MapRange<float>::in_InRangeB,
		&Beyond::SoundGraph::MapRange<float>::in_OutRangeA,
		&Beyond::SoundGraph::MapRange<float>::in_OutRangeB,
		&Beyond::SoundGraph::MapRange<float>::in_Clamped),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::MapRange<float>::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::MapRange<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::MapRange<int>::in_In,
		&Beyond::SoundGraph::MapRange<int>::in_InRangeA,
		&Beyond::SoundGraph::MapRange<int>::in_InRangeB,
		&Beyond::SoundGraph::MapRange<int>::in_OutRangeA,
		&Beyond::SoundGraph::MapRange<int>::in_OutRangeB,
		&Beyond::SoundGraph::MapRange<int>::in_Clamped),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::MapRange<int>::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::WavePlayer,
	NODE_INPUTS(
		&Beyond::SoundGraph::WavePlayer::Play,
		&Beyond::SoundGraph::WavePlayer::Stop,
		&Beyond::SoundGraph::WavePlayer::in_WaveAsset,
		&Beyond::SoundGraph::WavePlayer::in_StartTime,
		&Beyond::SoundGraph::WavePlayer::in_Loop,
		&Beyond::SoundGraph::WavePlayer::in_NumberOfLoops),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::WavePlayer::out_OnPlay,
		&Beyond::SoundGraph::WavePlayer::out_OnFinish,
		&Beyond::SoundGraph::WavePlayer::out_OnLooped,
		&Beyond::SoundGraph::WavePlayer::out_PlaybackPosition,
		&Beyond::SoundGraph::WavePlayer::out_OutLeft,
		&Beyond::SoundGraph::WavePlayer::out_OutRight)
);

DESCRIBE_NODE(Beyond::SoundGraph::Get<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Get<float>::Trigger,
		&Beyond::SoundGraph::Get<float>::in_Array,
		&Beyond::SoundGraph::Get<float>::in_Index),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Get<float>::out_OnTrigger,
		&Beyond::SoundGraph::Get<float>::out_Element)
);

DESCRIBE_NODE(Beyond::SoundGraph::Get<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Get<int>::Trigger,
		&Beyond::SoundGraph::Get<int>::in_Array,
		&Beyond::SoundGraph::Get<int>::in_Index),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Get<int>::out_OnTrigger,
		&Beyond::SoundGraph::Get<int>::out_Element)
);

DESCRIBE_NODE(Beyond::SoundGraph::Get<int64_t>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Get<int64_t>::Trigger,
		&Beyond::SoundGraph::Get<int64_t>::in_Array,
		&Beyond::SoundGraph::Get<int64_t>::in_Index),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Get<int64_t>::out_OnTrigger,
		&Beyond::SoundGraph::Get<int64_t>::out_Element)
);

DESCRIBE_NODE(Beyond::SoundGraph::GetRandom<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::GetRandom<float>::Next,
		&Beyond::SoundGraph::GetRandom<float>::Reset,
		&Beyond::SoundGraph::GetRandom<float>::in_Array,
		&Beyond::SoundGraph::GetRandom<float>::in_Seed),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::GetRandom<float>::out_OnNext,
		&Beyond::SoundGraph::GetRandom<float>::out_OnReset,
		&Beyond::SoundGraph::GetRandom<float>::out_Element)
);

DESCRIBE_NODE(Beyond::SoundGraph::GetRandom<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::GetRandom<int>::Next,
		&Beyond::SoundGraph::GetRandom<int>::Reset,
		&Beyond::SoundGraph::GetRandom<int>::in_Array,
		&Beyond::SoundGraph::GetRandom<int>::in_Seed),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::GetRandom<int>::out_OnNext,
		&Beyond::SoundGraph::GetRandom<int>::out_OnReset,
		&Beyond::SoundGraph::GetRandom<int>::out_Element)
);

DESCRIBE_NODE(Beyond::SoundGraph::GetRandom<int64_t>,
	NODE_INPUTS(
		&Beyond::SoundGraph::GetRandom<int64_t>::Next,
		&Beyond::SoundGraph::GetRandom<int64_t>::Reset,
		&Beyond::SoundGraph::GetRandom<int64_t>::in_Array,
		&Beyond::SoundGraph::GetRandom<int64_t>::in_Seed),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::GetRandom<int64_t>::out_OnNext,
		&Beyond::SoundGraph::GetRandom<int64_t>::out_OnReset,
		&Beyond::SoundGraph::GetRandom<int64_t>::out_Element)
);

DESCRIBE_NODE(Beyond::SoundGraph::Random<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Random<int>::Next,
		&Beyond::SoundGraph::Random<int>::Reset,
		&Beyond::SoundGraph::Random<int>::in_Min,
		&Beyond::SoundGraph::Random<int>::in_Max,
		&Beyond::SoundGraph::Random<int>::in_Seed),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Random<int>::out_OnNext,
		&Beyond::SoundGraph::Random<int>::out_OnReset,
		&Beyond::SoundGraph::Random<int>::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::Random<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::Random<float>::Next,
		&Beyond::SoundGraph::Random<float>::Reset,
		&Beyond::SoundGraph::Random<float>::in_Min,
		&Beyond::SoundGraph::Random<float>::in_Max,
		&Beyond::SoundGraph::Random<float>::in_Seed),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Random<float>::out_OnNext,
		&Beyond::SoundGraph::Random<float>::out_OnReset,
		&Beyond::SoundGraph::Random<float>::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::Noise,
	NODE_INPUTS(
		&Beyond::SoundGraph::Noise::in_Seed,
		&Beyond::SoundGraph::Noise::in_Type),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Noise::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::Sine,
	NODE_INPUTS(
		&Beyond::SoundGraph::Sine::ResetPhase,
		&Beyond::SoundGraph::Sine::in_Frequency,
		&Beyond::SoundGraph::Sine::in_PhaseOffset),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::Sine::out_Sine)
);

DESCRIBE_NODE(Beyond::SoundGraph::ADEnvelope,
	NODE_INPUTS(
		&Beyond::SoundGraph::ADEnvelope::Trigger,
		&Beyond::SoundGraph::ADEnvelope::in_AttackTime,
		&Beyond::SoundGraph::ADEnvelope::in_DecayTime,
		&Beyond::SoundGraph::ADEnvelope::in_AttackCurve,
		&Beyond::SoundGraph::ADEnvelope::in_DecayCurve,
		&Beyond::SoundGraph::ADEnvelope::in_Looping),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::ADEnvelope::out_OnTrigger,
		&Beyond::SoundGraph::ADEnvelope::out_OnComplete,
		&Beyond::SoundGraph::ADEnvelope::out_OutEnvelope)
);


DESCRIBE_NODE(Beyond::SoundGraph::RepeatTrigger,
	NODE_INPUTS(
		&Beyond::SoundGraph::RepeatTrigger::Start,
		&Beyond::SoundGraph::RepeatTrigger::Stop,
		&Beyond::SoundGraph::RepeatTrigger::in_Period),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::RepeatTrigger::out_Trigger)
);

DESCRIBE_NODE(Beyond::SoundGraph::TriggerCounter,
	NODE_INPUTS(
		&Beyond::SoundGraph::TriggerCounter::Trigger,
		&Beyond::SoundGraph::TriggerCounter::Reset,
		&Beyond::SoundGraph::TriggerCounter::in_StartValue,
		&Beyond::SoundGraph::TriggerCounter::in_StepSize,
		&Beyond::SoundGraph::TriggerCounter::in_ResetCount),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::TriggerCounter::out_OnTrigger,
		&Beyond::SoundGraph::TriggerCounter::out_OnReset,
		&Beyond::SoundGraph::TriggerCounter::out_Count,
		&Beyond::SoundGraph::TriggerCounter::out_Value)
);

DESCRIBE_NODE(Beyond::SoundGraph::DelayedTrigger,
	NODE_INPUTS(
		&Beyond::SoundGraph::DelayedTrigger::Trigger,
		&Beyond::SoundGraph::DelayedTrigger::Reset,
		&Beyond::SoundGraph::DelayedTrigger::in_DelayTime),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::DelayedTrigger::out_DelayedTrigger,
		&Beyond::SoundGraph::DelayedTrigger::out_OnReset)
);


DESCRIBE_NODE(Beyond::SoundGraph::BPMToSeconds,
	NODE_INPUTS(
		&Beyond::SoundGraph::BPMToSeconds::in_BPM),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::BPMToSeconds::out_Seconds)
);

DESCRIBE_NODE(Beyond::SoundGraph::NoteToFrequency<float>,
	NODE_INPUTS(
		&Beyond::SoundGraph::NoteToFrequency<float>::in_MIDINote),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::NoteToFrequency<float>::out_Frequency)
);

DESCRIBE_NODE(Beyond::SoundGraph::NoteToFrequency<int>,
	NODE_INPUTS(
		&Beyond::SoundGraph::NoteToFrequency<int>::in_MIDINote),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::NoteToFrequency<int>::out_Frequency)
);

DESCRIBE_NODE(Beyond::SoundGraph::FrequencyToNote,
	NODE_INPUTS(
		&Beyond::SoundGraph::FrequencyToNote::in_Frequency),
	NODE_OUTPUTS(
		&Beyond::SoundGraph::FrequencyToNote::out_MIDINote)
);

#include "choc/text/choc_StringUtilities.h"
#include <optional>

//=============================================================================
/**
	Utilities to procedurally registerand initialize node processor endpoints.
*/
namespace Beyond::SoundGraph::EndpointUtilities
{
	namespace Impl
	{
		// TODO: remove  prefix from the members, maybe keep in_ / out_
		constexpr std::string_view RemovePrefixAndSuffix(std::string_view name)
		{
			if (Beyond::Utils::StartsWith(name, "in_"))
				name.remove_prefix(sizeof("in_") - 1);
			else if (Beyond::Utils::StartsWith(name, "out_"))
				name.remove_prefix(sizeof("out_") - 1);
			
			return name;
		}

		//=============================================================================
		/// Implementation of the RegisterEndpoints function. Parsing type data into
		/// node processor enpoints.
		template<typename T>
		static bool RegisterEndpointInputsImpl(NodeProcessor* node, T& v, std::string_view memberName)
		{
			using TMember = T;
			constexpr bool isInputEvent = std::is_member_function_pointer_v<T>;
			//constexpr bool isOutputEvent = std::is_same_v<TMember, Beyond::SoundGraph::NodeProcessor::OutputEvent>;

			if constexpr (isInputEvent)
			{
			}
			/*else if constexpr (isOutputEvent)
			{
				node->AddOutEvent(Identifier(RemovePrefixAndSuffix(memberName)), v);
			}*/
			else
			{
				//const bool isArray = std::is_array_v<TMember>;
				//using TMemberDecay = std::conditional_t<isArray, std::remove_pointer_t<std::decay_t<TMember>>, std::decay_t<TMember>>;

				//const bool isArray = Type::is_array_v<TMember> || std::is_array_v<TMember> || Beyond::Utils::StartsWith(pinName, "in_Array") || Beyond::Utils::StartsWith(pinName, "out_Array");
				//constexpr bool isInput = /*isArray ? Beyond::Utils::StartsWith(pinName, "in_") : */std::is_pointer_v<TMember>;

				//if constexpr (isInput)
				{
					node->AddInStream(Identifier(RemovePrefixAndSuffix(memberName)));
				}
			}

			return true;
		}

		template<typename T>
		static bool RegisterEndpointOutputsImpl(NodeProcessor* node, T& v, std::string_view memberName)
		{
			using TMember = T;
			constexpr bool isOutputEvent = std::is_same_v<TMember, NodeProcessor::OutputEvent>;

			if constexpr (isOutputEvent)
			{
				node->AddOutEvent(Identifier(RemovePrefixAndSuffix(memberName)), v);
			}
			else
			{
				node->AddOutStream<TMember>(Identifier(RemovePrefixAndSuffix(memberName)), v);
			}

			return true;
		}

		template<typename T>
		static bool InitializeInputsImpl(NodeProcessor* node, T& v, std::string_view memberName)
		{
			using TMember = T;
			constexpr bool isInputEvent = std::is_member_function_pointer_v<T>;
			constexpr bool isOutputEvent = std::is_same_v<TMember, NodeProcessor::OutputEvent>;

			//? DBG
			//std::string_view str = typeid(TMember).name();

			if constexpr (isInputEvent || isOutputEvent)
			{
			}
			else
			{
				//const bool isArray = std::is_array_v<TMember>;
				//using TMemberDecay = std::conditional_t<isArray, std::remove_pointer_t<std::decay_t<TMember>>, std::decay_t<TMember>>;

				//const bool isArray = Type::is_array_v<TMember> || std::is_array_v<TMember> || Beyond::Utils::StartsWith(pinName, "in_Array") || Beyond::Utils::StartsWith(pinName, "out_Array");
				constexpr bool isInput = /*isArray ? Beyond::Utils::StartsWith(pinName, "in_") : */std::is_pointer_v<TMember>;

				if constexpr (isInput)
					v = (TMember)node->InValue(Identifier(RemovePrefixAndSuffix(memberName))).getRawData();
			}

			return true;
		}

		//=============================================================================
		template<typename TNodeType>
		static bool RegisterEndpoints(TNodeType* node)
		{
			static_assert(DescribedNode<TNodeType>::value);
			using InputsDescription = typename NodeDescription<TNodeType>::Inputs;
			using OutputsDescription = typename NodeDescription<TNodeType>::Outputs;

			const bool insResult = InputsDescription::ApplyToStaticType(
				[&node](const auto&... members)
				{
					auto unpack = [&node, memberIndex = 0](auto memberPtr) mutable
					{
						using TMember = std::remove_reference_t<decltype(memberPtr)>;
						constexpr bool isInputEvent = std::is_member_function_pointer_v<TMember>;
						const std::string_view name = InputsDescription::MemberNames[memberIndex++];

						if constexpr (isInputEvent)
						{
							// TODO: hook up fFlags (?)
							return true;
						}
						else // output events also go here because they are wrapped into a callable object
						{
							return RegisterEndpointInputsImpl(node, node->*memberPtr, name);
						}

						return true;
					};

					return (unpack(members) && ...);
				});

			const bool outsResult = OutputsDescription::ApplyToStaticType(
				[&node](const auto&... members)
				{
					auto unpack = [&node, memberIndex = 0](auto memberPtr) mutable
					{
						using TMember = std::remove_reference_t<decltype(memberPtr)>;
						constexpr bool isInputEvent = std::is_member_function_pointer_v<TMember>;
						const std::string_view name = OutputsDescription::MemberNames[memberIndex++];

						if constexpr (isInputEvent)
						{
							return true;
						}
						else // output events also go here because they are wrapped into a callable object
						{
							return RegisterEndpointOutputsImpl(node, node->*memberPtr, name);
						}

						return true;
					};

					return (unpack(members) && ...);
				});

			return insResult && outsResult;
		}

		//=============================================================================
		template<typename TNodeType>
		static bool InitializeInputs(TNodeType* node)
		{
			static_assert(DescribedNode<TNodeType>::value);
			using InputsDescription = typename NodeDescription<TNodeType>::Inputs;

			return InputsDescription::ApplyToStaticType(
				[&node](const auto&... members)
				{
					auto unpack = [&node, memberIndex = 0](auto memberPtr) mutable
					{
						using TMember = decltype(memberPtr);
						constexpr bool isInputEvent = std::is_member_function_pointer_v<TMember>;
						const std::string_view name = InputsDescription::MemberNames[memberIndex++];
						
						//? DBG
						//std::string_view str = typeid(TMember).name();

						if constexpr (isInputEvent)
							return true;
						else
							return InitializeInputsImpl(node, node->*memberPtr, name);
					};
					return (unpack(members) && ...);
				});
		}

	} // namespace Impl

	template<typename TNodeType>
	static bool RegisterEndpoints(TNodeType* node)
	{
		return Impl::RegisterEndpoints(node);
	}

	template<typename TNodeType>
	static bool InitializeInputs(TNodeType* node)
	{
		return Impl::InitializeInputs(node);
	}

} // Beyond::SoundGraph::EndpointUtilities
