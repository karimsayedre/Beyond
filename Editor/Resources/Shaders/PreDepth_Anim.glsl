// Pre-depth shader

#version 450 core
#pragma stage : vert

#include <Buffers.glslh>

// Vertex buffer
layout(location = 0) in vec3 a_Position;

// Bone influences
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

layout(location = 0) out vec4 v_CurrentClipPosition;
layout(location = 1) out vec4 v_PreviousClipPosition;

layout(push_constant) uniform BoneTransformIndex
{
	uint Base;
	uint DrawIndex;
}
u_BoneTransformIndex;

// layout(location = 0) out float LinearDepth;

// Make sure both shaders compute the exact same answer(PBR shader).
// We need to have the same exact calculations to produce the gl_Position value (eg. matrix multiplications).
precise invariant gl_Position;

void main()
{
	vec4 modelMatrix[3] = r_Transforms.Transform[u_BoneTransformIndex.DrawIndex + gl_InstanceIndex].ModelMatrix;
	mat4 transform = mat4(vec4(modelMatrix[0].x, modelMatrix[1].x, modelMatrix[2].x, 0.0), vec4(modelMatrix[0].y, modelMatrix[1].y, modelMatrix[2].y, 0.0),
	                      vec4(modelMatrix[0].z, modelMatrix[1].z, modelMatrix[2].z, 0.0), vec4(modelMatrix[0].w, modelMatrix[1].w, modelMatrix[2].w, 1.0));

	mat4 boneTransform = r_BoneTransforms.BoneTransforms[(u_BoneTransformIndex.Base + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[0]] * a_BoneWeights[0];
	boneTransform += r_BoneTransforms.BoneTransforms[(u_BoneTransformIndex.Base + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[1]] * a_BoneWeights[1];
	boneTransform += r_BoneTransforms.BoneTransforms[(u_BoneTransformIndex.Base + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[2]] * a_BoneWeights[2];
	boneTransform += r_BoneTransforms.BoneTransforms[(u_BoneTransformIndex.Base + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[3]] * a_BoneWeights[3];

	vec4 prevModelMatrix[3] = r_Transforms.Transform[u_BoneTransformIndex.DrawIndex + gl_InstanceIndex].PrevModelMatrix;
	mat4 prevTransform = mat4(vec4(prevModelMatrix[0].x, prevModelMatrix[1].x, prevModelMatrix[2].x, 0.0), vec4(prevModelMatrix[0].y, prevModelMatrix[1].y, prevModelMatrix[2].y, 0.0),
	                          vec4(prevModelMatrix[0].z, prevModelMatrix[1].z, prevModelMatrix[2].z, 0.0), vec4(prevModelMatrix[0].w, prevModelMatrix[1].w, prevModelMatrix[2].w, 1.0));

	vec4 worldPosition = transform * boneTransform * vec4(a_Position, 1.0);
	vec4 prevWorldPosition = prevTransform * boneTransform * vec4(a_Position, 1.0);

	v_CurrentClipPosition = u_Camera.ViewProjectionMatrix * worldPosition;
	v_PreviousClipPosition = u_Camera.PrevViewProjectionMatrix * prevWorldPosition;

	// Near and far are flipped for better precision.
	// Only change along with the PBR shader.
	gl_Position = u_Camera.ViewProjectionMatrix * worldPosition;
}

#version 450 core
#pragma stage : frag
#include <Buffers.glslh>

layout(location = 0) in vec4 v_CurrentClipPosition;
layout(location = 1) in vec4 v_PreviousClipPosition;

layout(location = 0) out vec4 o_MotionVector;

void main()
{
	vec2 currentNDC = v_CurrentClipPosition.xy / v_CurrentClipPosition.w;
	vec2 previousNDC = v_PreviousClipPosition.xy / v_PreviousClipPosition.w;

	if (v_CurrentClipPosition.w <= 0 || v_PreviousClipPosition.w <= 0)
	{
		o_MotionVector = vec4(0.0, 0.0, 0.0, 0.0);
		return;
	}
	// Motion vector is the difference between the current and previous positions in view space
	vec2 motionVector = (previousNDC - currentNDC) * u_Camera.ClipToRenderTargetScale;
	motionVector.xy += vec2(u_Camera.CurrentJitter.x - u_Camera.PreviousJitter.x, -(u_Camera.CurrentJitter.y - u_Camera.PreviousJitter.y));
	motionVector.y *= -1.0; // Invert Y axis to match Vulkan coordinate system

	// Motion vector is the difference between current and previous normalized device coordinates (NDC)
	o_MotionVector = vec4(motionVector, 0.0, 1.0);
}
