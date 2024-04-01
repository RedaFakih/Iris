#version 450 core
#stage vertex

// Vertex Attributes
layout(location = 0) in vec3 a_Position;

// Transforms Buffer
layout(location = 1) in vec4 a_MatrixRow0;
layout(location = 2) in vec4 a_MatrixRow1;
layout(location = 3) in vec4 a_MatrixRow2;

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

// Make sure both the depth shader and the PBR shader compute the exact same result
precise invariant gl_Position;

void main()
{
	mat4 transform = mat4(
		vec4(a_MatrixRow0.x, a_MatrixRow1.x, a_MatrixRow2.x, 0.0f),
		vec4(a_MatrixRow0.y, a_MatrixRow1.y, a_MatrixRow2.y, 0.0f),
		vec4(a_MatrixRow0.z, a_MatrixRow1.z, a_MatrixRow2.z, 0.0f),
		vec4(a_MatrixRow0.w, a_MatrixRow1.w, a_MatrixRow2.w, 1.0f)
	);

	gl_Position = u_Camera.ViewProjectionMatrix * transform * vec4(a_Position, 1.0f);
}

#version 450 core
#stage fragment

void main()
{
    // Only write to depth.
}