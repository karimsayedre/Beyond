// Spot Shadow Map shader

#version 450 core
#pragma stage : vert

#include <Buffers.glslh>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;

// Bone influences
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

layout(push_constant) uniform PushConstants
{
	uint LightIndex;
	uint BoneTransformBaseIndex;
	uint DrawIndex;
}
u_Constants;

void main()
{
	vec4 modelMatrix[3] = r_Transforms.Transform[u_Constants.DrawIndex + gl_InstanceIndex].ModelMatrix;
	mat4 transform = mat4(vec4(modelMatrix[0].x, modelMatrix[1].x, modelMatrix[2].x, 0.0), vec4(modelMatrix[0].y, modelMatrix[1].y, modelMatrix[2].y, 0.0),
	                      vec4(modelMatrix[0].z, modelMatrix[1].z, modelMatrix[2].z, 0.0), vec4(modelMatrix[0].w, modelMatrix[1].w, modelMatrix[2].w, 1.0));

	mat4 boneTransform = r_BoneTransforms.BoneTransforms[(u_Constants.BoneTransformBaseIndex + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[0]] * a_BoneWeights[0];
	boneTransform += r_BoneTransforms.BoneTransforms[(u_Constants.BoneTransformBaseIndex + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[1]] * a_BoneWeights[1];
	boneTransform += r_BoneTransforms.BoneTransforms[(u_Constants.BoneTransformBaseIndex + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[2]] * a_BoneWeights[2];
	boneTransform += r_BoneTransforms.BoneTransforms[(u_Constants.BoneTransformBaseIndex + gl_InstanceIndex) * MAX_BONES + a_BoneIndices[3]] * a_BoneWeights[3];

	gl_Position = u_SpotLightMatrices.Mats[u_Constants.LightIndex] * transform * boneTransform * vec4(a_Position, 1.0);
}

#version 450 core
#pragma stage : frag

void main()
{
	// TODO: Check for alpha in texture
}