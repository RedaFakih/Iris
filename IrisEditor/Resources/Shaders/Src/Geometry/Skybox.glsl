#version 450 core
#stage vertex

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(std140, set = 1, binding = 0) uniform Camera
{
	mat4 ViewProjectionMatrix;
	mat4 InverseViewProjectionMatrix;
	mat4 ProjectionMatrix;
	mat4 InverseProjectionMatrix;
	mat4 ViewMatrix;
	mat4 InverseViewMatrix;
	vec2 DepthUnpackConsts;
} u_Camera;

layout(location = 0) out vec3 v_Position;

void main()
{
	vec4 position = vec4(a_Position.xy, 0.0f, 1.0f);

	v_Position = mat3(u_Camera.InverseViewMatrix) * vec3(u_Camera.InverseProjectionMatrix * position);
	// Silence unused warnings...
	gl_Position = vec4(a_TexCoord, 0.0f, 1.0f);
	gl_Position = position;
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec3 v_Position;

layout(set = 3, binding = 0) uniform samplerCube u_Texture;

layout(push_constant) uniform Uniforms
{
	float TextureLod;
	float Intensity;
} u_Uniforms;

void main()
{
	o_Color = textureLod(u_Texture, v_Position, u_Uniforms.TextureLod) * u_Uniforms.Intensity;
	o_Color.a = 1.0f;
}