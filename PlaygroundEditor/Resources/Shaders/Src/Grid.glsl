#version 450 core
#stage vertex

// Reference: https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/

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

layout(location = 0) out vec3 v_NearPoint;
layout(location = 1) out vec3 v_FarPoint;

vec3 UnProjectPoint(vec3 pos)
{
	vec4 unProjectedPoint = u_Camera.InverseViewMatrix * u_Camera.InverseProjectionMatrix * vec4(pos.xyz, 1.0f);
	return unProjectedPoint.xyz / unProjectedPoint.w;
}

void main()
{
	v_NearPoint = UnProjectPoint(vec3(a_Position.xy, 1.0f));
	v_FarPoint = UnProjectPoint(vec3(a_Position.xy, 0.0f));

	// Silence unused attribute warning...
	gl_Position = vec4(a_TexCoord, 0.0f, 1.0f);	
	gl_Position = vec4(a_Position, 1.0f);
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec3 v_NearPoint;
layout(location = 1) in vec3 v_FarPoint;

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

layout(push_constant) uniform Uniforms
{
	float Scale;
} u_Uniforms;

vec4 Grid(vec3 fragPos, float scale)
{
	vec2 coord = fragPos.xz * scale;
	vec2 derivative = fwidth(coord * 0.8f);

	vec2 grid = abs(fract(coord - 0.5f) - 0.5f) / derivative;

	float line = min(grid.x, grid.y);

	vec4 color = vec4(0.3f, 0.3f, 0.3f, 1.0f - min(line, 1.5f));

	float minX = min(derivative.x, 2.0f);
	float minZ = min(derivative.y, 2.0f);

	// X axis
	if (fragPos.z > -0.25f * minZ && fragPos.z < 0.25f * minZ)
		color = vec4(1.0f, 0.0f, 0.0f, 1.0f);

	// Z axis
	if (fragPos.x > -0.25f * minX && fragPos.x < 0.25f * minX)
		color = vec4(0.0f, 0.0f, 1.0f, 1.0f);

	return color;
}

float ComputeDepth(vec3 pos)
{
	vec4 clipSpacePos = u_Camera.ViewProjectionMatrix * vec4(pos, 1.0f);
	return clipSpacePos.z / clipSpacePos.w;
}

// From XeGTAO
float LinearizeDepth(const float screenDepth)
{
	float depthLinearizeMul = u_Camera.DepthUnpackConsts.x;
	float depthLinearizeAdd = u_Camera.DepthUnpackConsts.y;
	// Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) * cameraClipFar"
	return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

void main()
{
	float t = (-v_NearPoint.y) / (v_FarPoint.y - v_NearPoint.y);
	vec3 fragPos = v_NearPoint + t * (v_FarPoint - v_NearPoint);

	float fragmentDepth = ComputeDepth(fragPos);

	gl_FragDepth = fragmentDepth;

	float linearDepth = 1.0f / LinearizeDepth(fragmentDepth);
	float fading = max(0.0f, linearDepth + 0.02f);

	// Add resolution on the 1x1 squares
	o_Color = (Grid(fragPos, u_Uniforms.Scale) + Grid(fragPos, 1.0f)) * float(t > 0.0f);
	o_Color.a *= fading;

	if (o_Color.a <= 0.0f)
		discard;
}