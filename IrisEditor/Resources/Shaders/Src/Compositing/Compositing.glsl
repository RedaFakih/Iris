#version 450 core
#stage vertex

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

layout(set = 2, binding = 0) uniform sampler2D u_Texture;
layout(set = 2, binding = 1) uniform sampler2D u_BloomTexture;
layout(set = 2, binding = 2) uniform sampler2D u_BloomDirtTexture;

layout(std430, set = 1, binding = 9) readonly buffer LuminanceHistogramBuffer
{
	uint Bins[];
} s_LuminanceHistogram;

layout(push_constant) uniform Uniforms
{
    float Exposure;
	float BloomIntensity;
	float BloomDirtIntensity;
	float BloomUpsampleScale;
    float Opacity;
    float Time;
	int TonemapOperator;
	int AdaptiveExposure;
} u_Uniforms;

vec3 UpsampleTent9(sampler2D tex, float lod, vec2 uv, vec2 texelSize, float radius)
{
	vec4 offset = texelSize.xyxy * vec4(1.0f, 1.0f, -1.0f, 0.0f) * radius;

	// Center
	vec3 result = textureLod(tex, uv, lod).rgb * 4.0f;

	result += textureLod(tex, uv - offset.xy, lod).rgb;
	result += textureLod(tex, uv - offset.wy, lod).rgb * 2.0f;
	result += textureLod(tex, uv - offset.zy, lod).rgb;

	result += textureLod(tex, uv + offset.zw, lod).rgb * 2.0f;
	result += textureLod(tex, uv + offset.xw, lod).rgb * 2.0f;

	result += textureLod(tex, uv + offset.zy, lod).rgb;
	result += textureLod(tex, uv + offset.wy, lod).rgb * 2.0f;
	result += textureLod(tex, uv + offset.xy, lod).rgb;

	return result * (1.0f / 16.0f);
}

// Based on http://www.oscars.org/science-technology/sci-tech-projects/aces
vec3 ACESTonemap(vec3 color)
{
	mat3 m1 = mat3(
		0.59719f, 0.07600f, 0.02840f,
		0.35458f, 0.90834f, 0.13383f,
		0.04823f, 0.01566f, 0.83777f
	);
	mat3 m2 = mat3(
		 1.60475f, -0.10208f, -0.00327f,
		-0.53108f,  1.10813f, -0.07276f,
		-0.07367f, -0.00605f,  1.07602f
	);
	vec3 v = m1 * color;
	vec3 a = v * (v + 0.0245786f) - 0.000090537f;
	vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return clamp(m2 * (a / b), 0.0f, 1.0f);
}

vec3 FilmicTonemap(vec3 x) {
  vec3 X = max(vec3(0.0f), x - 0.004f);
  vec3 result = (X * (6.2f * X + 0.5f)) / (X * (6.2f * X + 1.7f) + 0.06f);
  return pow(result, vec3(2.2f));
}

const mat3 LINEAR_REC2020_TO_LINEAR_SRGB = mat3(
	1.6605f, -0.1246f, -0.0182f,
	-0.5876f, 1.1329f, -0.1006f,
	-0.0728f, -0.0083f, 1.1187f
);

const mat3 LINEAR_SRGB_TO_LINEAR_REC2020 = mat3(
	0.6274f, 0.0691f, 0.0164f,
	0.3293f, 0.9195f, 0.0880f,
	0.0433f, 0.0113f, 0.8956f
);

const mat3 AgXInsetMatrix = mat3(
	0.856627153315983f, 0.137318972929847f, 0.11189821299995f,
	0.0951212405381588f, 0.761241990602591f, 0.0767994186031903f,
	0.0482516061458583f, 0.101439036467562f, 0.811302368396859f
);

const mat3 AgXOutsetMatrix = mat3(
	1.1271005818144368f, -0.1413297634984383f, -0.14132976349843826f,
	-0.11060664309660323f, 1.157823702216272f, -0.11060664309660294f,
	-0.016493938717834573f, -0.016493938717834257f, 1.2519364065950405f
);

const float AgxMinEv = -12.47393f;
const float AgxMaxEv = 4.026069f;

vec3 agxAscCdl(vec3 color, vec3 slope, vec3 offset, vec3 power, float sat) {
	const vec3 lw = vec3(0.2126f, 0.7152f, 0.0722f);
	float luma = dot(color, lw);
	vec3 c = pow(color * slope + offset, power);
	return luma + sat * (c - luma);
}

vec3 AGXTonemap(vec3 color) {
	color = LINEAR_SRGB_TO_LINEAR_REC2020 * color; // From three.js

	// 1. agx()
	// Input transform (inset)
	color = AgXInsetMatrix * color;

	color = max(color, 1e-10); // From Filament: avoid 0 or negative numbers for log2

	// Log2 space encoding
	color = clamp(log2(color), AgxMinEv, AgxMaxEv);
	color = (color - AgxMinEv) / (AgxMaxEv - AgxMinEv);

	color = clamp(color, 0.0f, 1.0f); // From Filament

	// Apply sigmoid function approximation
	// Mean error^2: 3.6705141e-06
	vec3 x2 = color * color;
	vec3 x4 = x2 * x2;
	color = + 15.5f     * x4 * x2
			- 40.14f    * x4 * color
			+ 31.96f    * x4
			- 6.868f    * x2 * color
			+ 0.4298f   * x2
			+ 0.1191f   * color
			- 0.00232f;

	if (u_Uniforms.TonemapOperator == 4)
	{
		// Punchy
		color = agxAscCdl(color, vec3(1.0f), vec3(0.0f), vec3(1.35f), 1.4f);
	}

	// 3. agxEotf()
	// Inverse input transform (outset)
	color = AgXOutsetMatrix * color;

	// sRGB IEC 61966-2-1 2.2 Exponent Reference EOTF Display
	// NOTE: We're linearizing the output here. Comment/adjust when
	// *not* using a sRGB render target
	color = pow(max(vec3(0.0f), color), vec3(2.2f)); // From filament: max()

	color = LINEAR_REC2020_TO_LINEAR_SRGB * color; // From three.js
	// Gamut mapping. Simple clamp for now.
	color = clamp(color, 0.0f, 1.0f);

	return color;
}

vec3 ReinhardTonemap(vec3 x) {
  const float L_white = 4.0f;

  return (x * (1.0f + x / (L_white * L_white))) / (1.0f + x);
}

vec3 PbrNeutralTonemap(vec3 color) {
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, newPeak * vec3(1.0, 1.0, 1.0), g);
}

vec3 GammaCorrect(vec3 color, float gamma)
{
	return pow(color, vec3(1.0f / gamma));
}

void main()
{
	const float gamma = 2.2f;

	vec3 color = texture(u_Texture, v_TexCoord).rgb;

	ivec2 texSize = textureSize(u_BloomTexture, 0);
	vec2 fTexSize = vec2(float(texSize.x), float(texSize.y));
	if (u_Uniforms.BloomIntensity > 0.0f)
	{
		vec3 bloom = UpsampleTent9(u_BloomTexture, 0, v_TexCoord, 1.0f / fTexSize, u_Uniforms.BloomUpsampleScale) * u_Uniforms.BloomIntensity;
		vec3 bloomDirt = texture(u_BloomDirtTexture, v_TexCoord).rgb * u_Uniforms.BloomDirtIntensity;

		color += bloom;
		color += bloom * bloomDirt;
	}

	float exposure = u_Uniforms.Exposure;

	if (u_Uniforms.AdaptiveExposure == 1u)
	{
		exposure *= uintBitsToFloat(s_LuminanceHistogram.Bins[IR_LUMINANCE_BUFFER_OFFSET_EXPOSURE / 4]);
	}

	color *= exposure;

	// Grain
	// float grainStrength = 5.0f;
	// float x = (v_TexCoord.x + 1.0f) * (v_TexCoord.y + 1.0f) * u_Uniforms.Time;
	// float grain = mod((mod(x, 13.0f) + 1.0f) * (mod(x, 123.0f) + 1.0f), 0.01f) - 0.006f;
	// color += grain * grainStrength;

	switch (u_Uniforms.TonemapOperator)
	{
		case 0: color = ACESTonemap(color); break;
		case 1: color = ReinhardTonemap(color); break;
		case 2: color = FilmicTonemap(color); break;
		case 3: color = AGXTonemap(color); break;
		case 4: color = AGXTonemap(color); break;
		case 5: color = PbrNeutralTonemap(color); break;
		default: color = PbrNeutralTonemap(color); break;
	}

	color = GammaCorrect(color, gamma);
	color *= u_Uniforms.Opacity;

	o_Color = vec4(color, 1.0f);
}