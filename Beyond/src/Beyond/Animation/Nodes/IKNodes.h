#pragma once

#include "Beyond/Animation/NodeDescriptor.h"
#include "Beyond/Animation/NodeProcessor.h"

#include <glm/glm.hpp>

namespace Beyond::AnimationGraph {

	struct AimIK final : public NodeProcessor
	{
		AimIK(const char* dbgName, UUID id);

		void Init(const Skeleton* skeleton) override;
		float Process(float timestemp) override;

		// WARNING: changing the names of these variables will break saved graphs
		// input variable pointers here...
		Pose *in_Pose = &DefaultPose;
		glm::vec3* in_Target = &DefaultTarget;         // target (world space) to aim at
		float *in_Weight = &DefaultWeight;             // How much of the AimIK result is blended in to the output pose. A value of 0.0 will disable the AimIK completely (i.e. the solver does not run)
		int* in_Bone = &DefaultBone;
		glm::vec3* in_AimOffset = &DefaultAimOffset;   // offset (bone local space) from bone of point that should be aimed at target.  e.g this could be the tip of a weapon.
		glm::vec3* in_AimAxis = &DefaultAimAxis;       // AimIK seeks to aim this vector (bone local space) at the target.
		glm::vec3* in_PoleVector = &DefaultPoleVector; // Pole vector to rotate around
		int* in_ChainLength = &DefaultChainLength;     // Number of bones in the IK chain.  1 = just the bone specified by Bone, 2 = Bone and its parent, 3 = Bone, its parent, and its grandparent, etc.
		float* in_ChainFactor = &DefaultChainFactor;   // Controls how much of the rotation needed to aim at target is transferred along the IK chain. 0 = nothing is transferred (effectively same as having chain length = 1), 1 = everything is transferred to the root of the chain)

		inline static Pose DefaultPose;
		inline static glm::vec3 DefaultTarget = { 0.0f, 0.0f, 0.0f };
		inline static float DefaultWeight = 0.0f;
		inline static int DefaultBone = 0;
		inline static glm::vec3 DefaultAimOffset = { 0.0f, 0.0f, 0.0f };
		inline static glm::vec3 DefaultAimAxis = { 0.0f, 0.0f, 1.0f };
		inline static float DefaultTwistDegrees = 0.0f;
		inline static glm::vec3 DefaultPoleVector = { 0.0f, 1.0f, 0.0f };
		inline static int DefaultChainLength = 1;
		inline static float DefaultChainFactor = 0.5f;

		choc::value::Value out_Pose = choc::value::Value(PoseType);

	private:
		const Skeleton* m_Skeleton = nullptr;
	};

} // namespace Beyond::AnimationGraph


DESCRIBE_NODE(Beyond::AnimationGraph::AimIK,
	NODE_INPUTS(
		&Beyond::AnimationGraph::AimIK::in_Pose,
		&Beyond::AnimationGraph::AimIK::in_Target,
		&Beyond::AnimationGraph::AimIK::in_Weight,
		&Beyond::AnimationGraph::AimIK::in_Bone,
		&Beyond::AnimationGraph::AimIK::in_AimOffset,
		&Beyond::AnimationGraph::AimIK::in_AimAxis,
		&Beyond::AnimationGraph::AimIK::in_PoleVector,
		&Beyond::AnimationGraph::AimIK::in_ChainLength,
		&Beyond::AnimationGraph::AimIK::in_ChainFactor
	),
	NODE_OUTPUTS(
		&Beyond::AnimationGraph::AimIK::out_Pose)
);
