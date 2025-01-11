#pragma once
#include "Beyond/Animation/Animation.h"
#include "Beyond/Animation/AnimationGraph.h"
#include "Beyond/Animation/NodeDescriptor.h"
#include "Beyond/Animation/PoseTrackWriter.h"

#include <acl/decompression/decompress.h>
#include <CDT/CDT.h>
#include <glm/glm.hpp>

#define DECLARE_ID(name) static constexpr Identifier name{ #name }

namespace Beyond::AnimationGraph {

	struct Prototype;

	struct BlendSpaceVertex : public NodeProcessor
	{
		BlendSpaceVertex(const char* dbgName, UUID id);

		void Init(const Skeleton*) override;
		void EnsureAnimation();
		void UpdateTime(float timestep);
		float Process(float timestep) override;
		float GetAnimationDuration() const override;
		float GetAnimationTimePos() const override;
		void SetAnimationTimePos(float timePos) override;

		// WARNING: changing the names of these variables will break saved graphs
		int64_t* in_Animation = &DefaultAnimation;
		bool* in_Synchronize = &DefaultSynchronize;

		// Runtime defaults for the above inputs.  Editor defaults are set to the same values (see AnimationGraphNodes.cpp)
		// Individual graphs can override these defaults, in which case the values are saved in this->DefaultValuePlugs
		inline static int64_t DefaultAnimation = 0;
		inline static bool DefaultSynchronize = true;

		choc::value::Value out_Pose = choc::value::Value(PoseType);

		float X = 0.0f;
		float Y = 0.0f;

	private:
		PoseTrackWriter m_TrackWriter;
		acl::decompression_context<acl::default_transform_decompression_settings> context;

		glm::vec3 m_RootTranslationStart;
		glm::vec3 m_RootTranslationEnd;
		glm::quat m_RootRotationStart;
		glm::quat m_RootRotationEnd;

		AssetHandle m_PreviousAnimation = 0;
		const Beyond::Animation* m_Animation = nullptr;

		float m_AnimationTimePos = 0.0f;
		float m_PreviousAnimationTimePos = 0.0f;
	};


	// Blend multiple animations together according to input coordinates in a 2D plane
	struct BlendSpace : public NodeProcessor
	{
		BlendSpace(const char* dbgName, UUID id);

		void Init(const Skeleton* skeleton) override;
		float Process(float timestep) override;
		void SetAnimationTimePos(float timePos) override;

		// WARNING: changing the names of these variables will break saved graphs
		float* in_X = &DefaultX;
		float* in_Y = &DefaultY;

		// Runtime defaults for the above inputs.  Editor defaults are set to the same values (see AnimationGraphNodes.cpp)
		// Individual graphs can override these defaults, in which case the values are saved in this->DefaultValuePlugs
		inline static float DefaultX = 0.0f;
		inline static float DefaultY = 0.0f;

		choc::value::Value out_Pose = choc::value::Value(PoseType);

	private:
		void LerpPosition(float timestep);

		void Blend1(CDT::IndexSizeType i0, float timestep, CDT::IndexSizeType& previousI0, CDT::IndexSizeType& previousI1, CDT::IndexSizeType& previousI2);
		void Blend2(CDT::IndexSizeType i0, CDT::IndexSizeType i1, float u, float v, float timestep, CDT::IndexSizeType& previousI0, CDT::IndexSizeType& previousI1, CDT::IndexSizeType& previousI2);
		void Blend3(CDT::IndexSizeType i0, CDT::IndexSizeType i1, CDT::IndexSizeType i2, float u, float v, float w, float timestep);

		CDT::Triangulation<float> m_Triangulation;
		std::vector<Scope<BlendSpaceVertex>> m_Vertices;

		CDT::IndexSizeType m_PreviousI0 = -1;
		CDT::IndexSizeType m_PreviousI1 = -1;
		CDT::IndexSizeType m_PreviousI2 = -1;

		float m_LerpSecondsPerUnitX = 0.0f; // 0 => instantly snap to new position
		float m_LerpSecondsPerUnitY = 0.0f;

		// Values from the last frame.
		// We do not need to recalculate if inputs are unchanged
		float m_LastX = -FLT_MAX;
		float m_LastY = -FLT_MAX;

		float m_U = 0.0f;
		float m_V = 0.0f;
		float m_W = 0.0f;

		const Skeleton* m_Skeleton;

		friend bool InitBlendSpace(BlendSpace*, const float, const float, const Prototype&);

	};


	// Blend two poses depending on a boolean condition.
	// Note that this node cannot "synchronize" the blend.
	// If you need synchronization, then use a node that blends animation clips rather than poses.
	// (such as BlendSpace or RangedBlend)
	struct ConditionalBlend : public NodeProcessor
	{
		ConditionalBlend(const char* dbgName, UUID id);

		void Init(const Skeleton*) override;
		float Process(float timestep) override;

		// WARNING: changing the names of these variables will break saved graphs
		Pose* in_BasePose = &DefaultPose;
		bool* in_Condition = &DefaultCondition;
		float* in_Weight = &DefaultWeight;
		bool* in_Additive = &DefaultAdditive;
		int* in_BlendRootBone = &DefaultBone;
		float* in_BlendInDuration = &DefaultBlendInDuration;
		float* in_BlendOutDuration = &DefaultBlendOutDuration;

		// Runtime defaults for the above inputs.  Editor defaults are set to the same values (see AnimationGraphNodes.cpp)
		// Individual graphs can override these defaults, in which case the values are saved in this->DefaultValuePlugs
		inline static Pose DefaultPose = {};
		inline static bool DefaultCondition = false;
		inline static float DefaultWeight = 1.0f;
		inline static bool DefaultAdditive = false;
		inline static int DefaultBone = 0;
		inline static float DefaultBlendInDuration = 0.1f;
		inline static float DefaultBlendOutDuration = 0.1f;

		choc::value::Value out_Pose = choc::value::Value(PoseType);

	private:
		Ref<AnimationGraph> m_AnimationGraph = nullptr;
		const Skeleton* m_Skeleton = nullptr;
		float m_TransitionWeight = 0.0f;
		bool m_LastCondition = false;

		friend bool InitConditionalBlend(Ref<AnimationGraph>, ConditionalBlend*, const Prototype&);
	};


	// Blend a secondary pose into a base pose together depending on a trigger.
	// The secondary is automatically blended out when it has played through once.
	// Note that this node is cannot "synchronize" the blend.
	// If you need synchronization, then use a node that blends animation clips rather than poses.
	// (such as BlendSpace or RangedBlend)
	struct OneShot : public NodeProcessor
	{
		struct IDs
		{
			DECLARE_ID(Trigger);
			DECLARE_ID(Finished);
		private:
			IDs() = delete;
		};

		OneShot(const char* dbgName, UUID id);

		void Init(const Skeleton*) override;
		float Process(float timestep) override;

		// WARNING: changing the names of these variables will break saved graphs
		Pose* in_BasePose = &DefaultPose;
		float* in_Weight = &DefaultWeight;
		bool* in_Additive = &DefaultAdditive;
		int* in_BlendRootBone = &DefaultBone;
		float* in_BlendInDuration = &DefaultBlendInDuration;
		float* in_BlendOutDuration = &DefaultBlendOutDuration;

		OutputEvent out_OnFinished;

		void Trigger(Identifier)
		{
			m_Trigger.SetDirty();
		}

		// Runtime defaults for the above inputs.  Editor defaults are set to the same values (see AnimationGraphNodes.cpp)
		// Individual graphs can override these defaults, in which case the values are saved in this->DefaultValuePlugs
		inline static Pose DefaultPose = {};
		inline static float DefaultWeight = 1.0f;
		inline static bool DefaultAdditive = false;
		inline static int DefaultBone = 0;
		inline static float DefaultBlendInDuration = 0.1f;
		inline static float DefaultBlendOutDuration = 0.1f;

		choc::value::Value out_Pose = choc::value::Value(PoseType);

	private:
		Ref<AnimationGraph> m_AnimationGraph = nullptr;
		const Skeleton* m_Skeleton = nullptr;
		float m_TriggeredDuration = 0.0f;
		Flag m_Trigger;
		bool m_Triggered = false;

		friend bool InitOneShot(Ref<AnimationGraph>, OneShot*, const Prototype&);
	};


	// Blend two animations together
	// Given animation A, animation B and a blend parameter, the animations are blended proprotional to
	// where the blend parameter falls within the range [Range A, Range B]
	struct RangedBlend : public NodeProcessor
	{
		struct IDs
		{
			DECLARE_ID(LoopA);
			DECLARE_ID(LoopB);
			DECLARE_ID(FinishA);
			DECLARE_ID(FinishB);
		private:
			IDs() = delete;
		};

		RangedBlend(const char* dbgName, UUID id);

		void Init(const Skeleton*) override;
		float Process(float timestep) override;
		void SetAnimationTimePos(float timePos) override;

		// WARNING: changing the names of these variables will break saved graphs
		int64_t* in_AnimationA = &DefaultAnimation;
		int64_t* in_AnimationB = &DefaultAnimation;
		float* in_PlaybackSpeedA = &DefaultPlaybackSpeed;
		float* in_PlaybackSpeedB = &DefaultPlaybackSpeed;
		bool* in_Synchronize = &DefaultSynchronize;
		float* in_OffsetA = &DefaultOffset;
		float* in_OffsetB = &DefaultOffset;
		bool* in_Loop = &DefaultLoop;
		float* in_RangeA = &DefaultRangeA;
		float* in_RangeB = &DefaultRangeB;
		float* in_Value = &DefaultValue;

		// Runtime defaults for the above inputs.  Editor defaults are set to the same values (see AnimationGraphNodes.cpp)
		// Individual graphs can override these defaults, in which case the values are saved in this->DefaultValuePlugs
		inline static int64_t DefaultAnimation = 0;
		inline static float DefaultPlaybackSpeed = 1.0f;
		inline static bool DefaultSynchronize = true;
		inline static float DefaultOffset = 0.0f;
		inline static bool DefaultLoop = true;
		inline static float DefaultRangeA = 0.0f;
		inline static float DefaultRangeB = 1.0f;
		inline static float DefaultValue = 0.0f;

		choc::value::Value out_Pose = choc::value::Value(PoseType);

		OutputEvent out_OnFinishA;
		OutputEvent out_OnLoopA;
		OutputEvent out_OnFinishB;
		OutputEvent out_OnLoopB;

	private:
		Pose m_PoseA;
		Pose m_PoseB;

		PoseTrackWriter m_TrackWriterA;
		acl::decompression_context<acl::default_transform_decompression_settings> contextA;

		PoseTrackWriter m_TrackWriterB;
		acl::decompression_context<acl::default_transform_decompression_settings> contextB;

		glm::vec3 m_RootTranslationStartA;
		glm::vec3 m_RootTranslationEndA;
		glm::quat m_RootRotationStartA;
		glm::quat m_RootRotationEndA;
		glm::vec3 m_RootTranslationStartB;
		glm::vec3 m_RootTranslationEndB;
		glm::quat m_RootRotationStartB;
		glm::quat m_RootRotationEndB;

		AssetHandle m_PreviousAnimationA = 0;
		const Animation* m_AnimationA = nullptr;
		AssetHandle m_PreviousAnimationB = 0;
		const Animation* m_AnimationB = nullptr;

		float m_AnimationTimePosA = 0.0f;
		float m_PreviousAnimationTimePosA = 0.0f;
		float m_PreviousOffsetA = 0.0f;

		float m_AnimationTimePosB = 0.0f;
		float m_PreviousAnimationTimePosB = 0.0f;
		float m_PreviousOffsetB = 0.0f;

		const Skeleton* m_Skeleton = nullptr;
	};

} // namespace Beyond::AnimationGraph


DESCRIBE_NODE(Beyond::AnimationGraph::BlendSpace,
	NODE_INPUTS(
		&Beyond::AnimationGraph::BlendSpace::in_X,
		&Beyond::AnimationGraph::BlendSpace::in_Y),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::BlendSpace::out_Pose)
);


DESCRIBE_NODE(Beyond::AnimationGraph::BlendSpaceVertex,
	NODE_INPUTS(
		&Beyond::AnimationGraph::BlendSpaceVertex::in_Animation,
		&Beyond::AnimationGraph::BlendSpaceVertex::in_Synchronize),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::BlendSpaceVertex::out_Pose)
);


DESCRIBE_NODE(Beyond::AnimationGraph::ConditionalBlend,
	NODE_INPUTS(
		&Beyond::AnimationGraph::ConditionalBlend::in_BasePose,
		&Beyond::AnimationGraph::ConditionalBlend::in_Condition,
		&Beyond::AnimationGraph::ConditionalBlend::in_Weight,
		&Beyond::AnimationGraph::ConditionalBlend::in_Additive,
		&Beyond::AnimationGraph::ConditionalBlend::in_BlendRootBone,
		&Beyond::AnimationGraph::ConditionalBlend::in_BlendInDuration,
		&Beyond::AnimationGraph::ConditionalBlend::in_BlendOutDuration),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::ConditionalBlend::out_Pose)
);


DESCRIBE_NODE(Beyond::AnimationGraph::OneShot,
	NODE_INPUTS(
		&Beyond::AnimationGraph::OneShot::in_BasePose,
		&Beyond::AnimationGraph::OneShot::Trigger,
		&Beyond::AnimationGraph::OneShot::in_Weight,
		&Beyond::AnimationGraph::OneShot::in_Additive,
		&Beyond::AnimationGraph::OneShot::in_BlendRootBone,
		&Beyond::AnimationGraph::OneShot::in_BlendInDuration,
		&Beyond::AnimationGraph::OneShot::in_BlendOutDuration),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::OneShot::out_OnFinished,
		&Beyond::AnimationGraph::OneShot::out_Pose)

);


DESCRIBE_NODE(Beyond::AnimationGraph::RangedBlend,
	NODE_INPUTS(
		&Beyond::AnimationGraph::RangedBlend::in_AnimationA,
		&Beyond::AnimationGraph::RangedBlend::in_AnimationB,
		&Beyond::AnimationGraph::RangedBlend::in_PlaybackSpeedA,
		&Beyond::AnimationGraph::RangedBlend::in_PlaybackSpeedB,
		&Beyond::AnimationGraph::RangedBlend::in_Synchronize,
		&Beyond::AnimationGraph::RangedBlend::in_OffsetA,
		&Beyond::AnimationGraph::RangedBlend::in_OffsetB,
		&Beyond::AnimationGraph::RangedBlend::in_Loop,
		&Beyond::AnimationGraph::RangedBlend::in_RangeA,
		&Beyond::AnimationGraph::RangedBlend::in_RangeB,
		&Beyond::AnimationGraph::RangedBlend::in_Value),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::RangedBlend::out_Pose,
		&Beyond::AnimationGraph::RangedBlend::out_OnFinishA,
		&Beyond::AnimationGraph::RangedBlend::out_OnLoopA,
		&Beyond::AnimationGraph::RangedBlend::out_OnFinishB,
		&Beyond::AnimationGraph::RangedBlend::out_OnLoopB)
);


#undef DECLARE_ID
