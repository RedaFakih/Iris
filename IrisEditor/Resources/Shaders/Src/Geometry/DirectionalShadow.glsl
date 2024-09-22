#version 450 core
#stage vertex

// We use Cascaded Shadow Mapping + Percentage Closer Soft Shadows (CSM + PCSS)
// Ref: https://developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf
// Ref: https://www.saschawillems.de/blog/2017/12/30/new-vulkan-example-cascaded-shadow-mapping/

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;

// Transforms Buffer
layout(location = 5) in vec4 a_MatrixRow0;
layout(location = 6) in vec4 a_MatrixRow1;
layout(location = 7) in vec4 a_MatrixRow2;

layout(std140, set = 1, binding = 3) uniform DirectionalShadowData
{
	mat4 DirectionalLightMatrices[4]; // View Projection Matrices
} u_DirShadow;

layout(push_constant) uniform Transform
{
	uint Cascade;
} u_Renderer;

void main()
{
	mat4 transform = mat4(
		vec4(a_MatrixRow0.x, a_MatrixRow1.x, a_MatrixRow2.x, 0.0f),
		vec4(a_MatrixRow0.y, a_MatrixRow1.y, a_MatrixRow2.y, 0.0f),
		vec4(a_MatrixRow0.z, a_MatrixRow1.z, a_MatrixRow2.z, 0.0f),
		vec4(a_MatrixRow0.w, a_MatrixRow1.w, a_MatrixRow2.w, 1.0f)
	);

	// Silence warnings
	gl_Position = vec4(a_Normal, 1.0f);
	gl_Position = vec4(a_Tangent, 1.0f);
	gl_Position = vec4(a_Binormal, 1.0f);
	gl_Position = vec4(a_TexCoord, 0.0f, 1.0f);
	gl_Position = u_DirShadow.DirectionalLightMatrices[u_Renderer.Cascade] * transform * vec4(a_Position, 1.0f);
}

#version 450 core
#stage fragment

void main()
{
	// Only write to depth
}