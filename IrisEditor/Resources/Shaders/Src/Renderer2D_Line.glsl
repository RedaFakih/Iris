#version 450 core
#stage vertex

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout (std140, set = 0, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
};

layout (push_constant) uniform Transform
{
	mat4 Transform;
} u_Renderer;

layout (location = 0) out vec4 v_Color;

void main()
{
	v_Color = a_Color;

	gl_Position = u_ViewProjection * u_Renderer.Transform * vec4(a_Position, 1.0);
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;

void main()
{
	o_Color = v_Color;
}