// Spot Shadow Map shader

#version 450 core
#pragma stage : vert

#include <Buffers.glslh>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;

layout(push_constant) uniform Transform
{
	int LightIndex;
	int DrawIndex;
}
u_Renderer;

void main()
{
	vec4 modelMatrix[3] = r_Transforms.Transform[u_Renderer.DrawIndex + gl_InstanceIndex].ModelMatrix;
	mat4 transform = mat4(vec4(modelMatrix[0].x, modelMatrix[1].x, modelMatrix[2].x, 0.0), vec4(modelMatrix[0].y, modelMatrix[1].y, modelMatrix[2].y, 0.0),
	                      vec4(modelMatrix[0].z, modelMatrix[1].z, modelMatrix[2].z, 0.0), vec4(modelMatrix[0].w, modelMatrix[1].w, modelMatrix[2].w, 1.0));

	gl_Position = u_SpotLightMatrices.Mats[u_Renderer.LightIndex] * transform * vec4(a_Position, 1.0);
}

#version 450 core
#pragma stage : frag

void main()
{
	// TODO: Check for alpha in texture
}
