#version 450 core
#stage vertex

// One pass Depth Of Field shader
// Ref: <https://blog.voxagon.se/2018/05/04/bokeh-depth-of-field-in-single-pass.html>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_TexCoord;

void main()
{
	v_TexCoord = a_TexCoord;
	gl_Position = vec4(a_Position.xy, 0.0f, 1.0f);
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec2 v_TexCoord;

layout(set = 2, binding = 1) uniform sampler2D u_Texture;
layout(set = 2, binding = 2) uniform sampler2D u_DepthTexture;

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
	vec2 DOFParams; // FocusDistance, BlurSize
} u_Uniforms;

const float GOLDEN_ANGLE = 2.39996323f;
const float MAX_BLUR_SIZE = 20.0f;
const float RAD_SCALE = 2.0f; // Smaller = nicer blur, larger = faster

float LinearizeDepth(const float screenDepth)
{
	float depthLinearizeMul = u_Camera.DepthUnpackConsts.x;
	float depthLinearizeAdd = u_Camera.DepthUnpackConsts.y;
	return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

float GetBlurSize(float depth, float focusPoint, float focusScale)
{
	float coc = clamp((1.0f / focusPoint - 1.0f / depth) * focusScale, -1.0f, 1.0f);
	return abs(coc) * MAX_BLUR_SIZE;
}

vec3 DepthOfField(vec2 texCoord, float focusPoint, float focusScale, vec2 texelSize)
{
	float centerDepth = LinearizeDepth(texture(u_DepthTexture, texCoord).r);
	float centerSize = GetBlurSize(centerDepth, focusPoint, focusScale);
	vec3 color = texture(u_Texture, texCoord).rgb;
	float tot = 1.0f;
	float radius = RAD_SCALE;
	for (float ang = 0.0f; radius < MAX_BLUR_SIZE; ang += GOLDEN_ANGLE)
	{
		vec2 tc = texCoord + vec2(cos(ang), sin(ang)) * texelSize * radius;
		vec3 sampleColor = texture(u_Texture, tc).rgb;
		float sampleDepth =  LinearizeDepth(texture(u_DepthTexture, tc).r);
		float sampleSize = GetBlurSize(sampleDepth, focusPoint, focusScale);
		if (sampleDepth > centerDepth)
			sampleSize = clamp(sampleSize, 0.0f, centerSize * 2.0f);
		float m = smoothstep(radius - 0.5f, radius + 0.5f, sampleSize);
		color += mix(color / tot, sampleColor, m);
		tot += 1.0f;
		radius += RAD_SCALE / radius;
	}
	return color /= tot;
}

void main()
{
	ivec2 texSize = textureSize(u_Texture, 0);
	vec2 fTexSize = vec2(float(texSize.x), float(texSize.y));

	float focusPoint = u_Uniforms.DOFParams.x;
	float blurScale = u_Uniforms.DOFParams.y;

	vec3 color = DepthOfField(v_TexCoord, focusPoint, blurScale, 1.0f / fTexSize);
	o_Color = vec4(color, 1.0f);
}