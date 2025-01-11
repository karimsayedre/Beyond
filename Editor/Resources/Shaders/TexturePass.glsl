#version 450 core
#pragma stage : vert
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

struct OutputBlock
{
	vec2 TexCoord;
};

layout (location = 0) out OutputBlock Output;

void main()
{
	vec4 position = vec4(a_Position.xy, 0.0, 1.0);
	Output.TexCoord = a_TexCoord;
	gl_Position = position;
}

#version 450 core
#pragma stage : frag

layout(location = 0) out vec4 o_Color;
layout(location = 1) out vec4 o_Unused0;
layout(location = 2) out vec4 o_Unused1;

struct OutputBlock
{
	vec2 TexCoord;
};

layout (location = 0) in OutputBlock Input;

layout (binding = 0) uniform sampler2D u_Texture;

void main()
{
	o_Color = texture(u_Texture, Input.TexCoord);
	o_Unused0 = vec4(0.f);
	o_Unused1 = vec4(0.f);
}
