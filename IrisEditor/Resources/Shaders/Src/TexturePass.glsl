#version 450 core
#stage vertex
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout (location = 0) out vec2 v_TexCoord;

void main()
{
	v_TexCoord = a_TexCoord;
	gl_Position = vec4(a_Position.xy, 0.0, 1.0);
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;

layout (location = 0) in vec2 v_TexCoord;

layout (set = 3, binding = 0) uniform sampler2D u_Texture;

void main()
{
	o_Color = texture(u_Texture, v_TexCoord);
}
