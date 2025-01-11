#version 450 core
#pragma stage : comp


layout(binding = 0) uniform sampler2D u_Texture;
layout(binding = 1, rgba32f) writeonly uniform image2D o_Color;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
	ivec2 coords = ivec2(gl_GlobalInvocationID.xy);
	imageStore(o_Color, coords, texelFetch(u_Texture, coords, 0));
}
