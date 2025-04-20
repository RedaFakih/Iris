#version 450 core
#stage vertex

// Ref: https://learnopengl.com/PBR/
// Ref: https://developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf // Shadows
// Ref: https://www.saschawillems.de/blog/2017/12/30/new-vulkan-example-cascaded-shadow-mapping/ // Shadows

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;

// Transforms Buffer
layout(location = 5) in vec4 a_MatrixRow0;
layout(location = 6) in vec4 a_MatrixRow1;
layout(location = 7) in vec4 a_MatrixRow2;

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

layout(std140, set = 1, binding = 3) uniform DirectionalShadowData
{
	mat4 DirectionalLightMatrices[4];
} u_DirShadow;

struct VertexOutput
{
	vec3 WorldPosition;
	vec3 Normal;
	vec2 TexCoord;
	mat3 WorldNormals;
	mat3 WorldTransform;
	vec3 Binormal;

	vec3 ShadowMapCoords[4];

	mat3 CameraView;
	vec3 ViewPosition;
};

layout(location = 0) out VertexOutput Output;

// NOTE(IMPORTANT): Causes jitters on Nvidia driver versions post 552.44
// Make sure both the PreDdepth shader and the PBR shader compute the exact same result
// invariant gl_Position;

void main()
{
	mat4 transform = mat4(
		vec4(a_MatrixRow0.x, a_MatrixRow1.x, a_MatrixRow2.x, 0.0f),
		vec4(a_MatrixRow0.y, a_MatrixRow1.y, a_MatrixRow2.y, 0.0f),
		vec4(a_MatrixRow0.z, a_MatrixRow1.z, a_MatrixRow2.z, 0.0f),
		vec4(a_MatrixRow0.w, a_MatrixRow1.w, a_MatrixRow2.w, 1.0f)
	);

    vec4 worldPosition = transform * vec4(a_Position, 1.0f);

    Output.WorldPosition = worldPosition.xyz;
    Output.Normal = mat3(transform) * a_Normal;
    Output.TexCoord = vec2(a_TexCoord.x, 1.0f - a_TexCoord.y); // Invert y texture coordinate
    Output.WorldNormals = mat3(transform) * mat3(a_Tangent, a_Binormal, a_Normal);
    Output.WorldTransform = mat3(transform);
	Output.Binormal = a_Binormal;
    
	vec4 shadowCoords[4];
	shadowCoords[0] = u_DirShadow.DirectionalLightMatrices[0] * vec4(Output.WorldPosition.xyz, 1.0f);
	shadowCoords[1] = u_DirShadow.DirectionalLightMatrices[1] * vec4(Output.WorldPosition.xyz, 1.0f);
	shadowCoords[2] = u_DirShadow.DirectionalLightMatrices[2] * vec4(Output.WorldPosition.xyz, 1.0f);
	shadowCoords[3] = u_DirShadow.DirectionalLightMatrices[3] * vec4(Output.WorldPosition.xyz, 1.0f);
	Output.ShadowMapCoords[0] = vec3(shadowCoords[0].xyz / shadowCoords[0].w);
	Output.ShadowMapCoords[1] = vec3(shadowCoords[1].xyz / shadowCoords[1].w);
	Output.ShadowMapCoords[2] = vec3(shadowCoords[2].xyz / shadowCoords[2].w);
	Output.ShadowMapCoords[3] = vec3(shadowCoords[3].xyz / shadowCoords[3].w);

    Output.CameraView = mat3(u_Camera.ViewMatrix);
    Output.ViewPosition = vec3(u_Camera.ViewMatrix * vec4(Output.WorldPosition, 1.0f));

    gl_Position = u_Camera.ViewProjectionMatrix * worldPosition;
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;
layout(location = 1) out vec4 o_ViewNormalsLuminance;
layout(location = 2) out vec4 o_MetalnessRoughness;

struct VertexOutput
{
	vec3 WorldPosition;
	vec3 Normal;
	vec2 TexCoord;
	mat3 WorldNormals;
	mat3 WorldTransform;
	vec3 Binormal;

	vec3 ShadowMapCoords[4];

	mat3 CameraView;
	vec3 ViewPosition;
};

layout(location = 0) in VertexOutput Input;

layout(set = 0, binding = 0) uniform sampler2D u_BRDFLutTexture;

layout(set = 2, binding = 0) uniform samplerCube u_RadianceMap;
layout(set = 2, binding = 1) uniform samplerCube u_IrradianceMap;

layout(set = 2, binding = 2) uniform sampler2DArray u_ShadowMap;

layout(set = 3, binding = 0) uniform sampler2D u_AlbedoTexture;
layout(set = 3, binding = 1) uniform sampler2D u_NormalTexture;
layout(set = 3, binding = 2) uniform sampler2D u_RoughnessTexture;
layout(set = 3, binding = 3) uniform sampler2D u_MetalnessTexture;

// Set = 1 has uniform buffers
struct DirectionalLight
{
	vec4 Direction; // The alpha channel is the ShadowOpacity
	vec4 Radiance; // The alpha channel is the multiplier of the directional light
	float LightSize;
};

layout(std140, set = 1, binding = 2) uniform SceneData
{
	DirectionalLight DirLight;
	vec3 CameraPosition; // Offset 48
	float EnvironmentMapIntensity; // This is used in the PBR shader and it mirrors the intensity that is used in the Skybox shader
} u_Scene;

struct PointLight
{
	vec3 Position;
	float Intensity;
	vec3 Radiance;
	float Radius;
	vec3 Padding0;
	float Falloff;
};

layout(std140, set = 1, binding = 4) uniform PointLightsData
{
	uint LightCount;
	PointLight Lights[IR_MAX_POINT_LIGHT_COUNT];
} u_PointLights;

layout(std430, set = 1, binding = 5) buffer VisiblePointLightIndicesBuffer
{
	int Indices[];
} s_VisiblePointLightIndicesBuffer;

struct SpotLight
{
	vec3 Position;
	float Intensity;
	vec3 Direction;
	float AngleAttenuation;
	vec3 Radiance;
	float Range;
	vec2 Padding0;
	float Angle;
	float Falloff;
};

layout(std140, set = 1, binding = 6) uniform SpotLightsData
{
	uint LightCount;
	SpotLight Lights[IR_MAX_SPOT_LIGHT_COUNT];
} u_SpotLights;

layout(std430, set = 1, binding = 7) writeonly buffer VisibleSpotLightIndicesBuffer
{
	int Indices[];
} s_VisibleSpotLightIndicesBuffer;

layout(std140, set = 1, binding = 8) uniform RendererData
{
    vec4 CascadeSplits;
	float Padding0;	// Pad here so that we dont get a 12 byte padding in the end
	float MaxShadowDistance;
	float ShadowFade;
	float CascadeTransitionFade;
	uint TilesCountX;
	bool CascadeFading;
	bool SoftShadows;
	bool ShowCascades;
	bool ShowLightComplexity;
	bool Unlit;
} u_RendererData;

layout(push_constant) uniform Materials
{
    vec3 AlbedoColor;
    float Roughness;
    float Metalness;
    float Emission;
	float Tiling;

	float EnvMapRotation;

    bool UseNormalMap;
} u_MaterialUniforms;

struct PBRParameters
{
	vec3 Albedo;
	float Roughness;
	float Metalness;

	vec3 Normal;
	vec3 View;
	float NdotV;
} m_Params;

const float PI = 3.14159265358979323846f;
const float Epsilon = 0.00001;
// Constant normal incidence Fresnel factor for all dielectrics.
const vec3 c_Fdielectric = vec3(0.04f);

////////////////////////////////////////////////////////////////////////////////////////////
// Shadow Mapping Functions
////////////////////////////////////////////////////////////////////////////////////////////

float g_ShadowFade = 1.0f; // TODO: A better placement of this?

vec3 GetShadowMapCoords(vec3 shadowMapCoords[4], uint cascade)
{
	switch (cascade)
	{
		case 0: return shadowMapCoords[0];
		case 1: return shadowMapCoords[1];
		case 2: return shadowMapCoords[2];
		case 3: return shadowMapCoords[3];
	}

	return vec3(0.0f);
}

float GetDirShadowBias()
{
	const float MINIMUM_SHADOW_BIAS = 0.002f;
	float bias = max(MINIMUM_SHADOW_BIAS * (1.0f - dot(m_Params.Normal, u_Scene.DirLight.Direction.xyz)), MINIMUM_SHADOW_BIAS);
	return bias;
}

float HardShadows_DirectionalLight(sampler2DArray shadowMap, uint cascade, vec3 shadowCoords)
{
	float bias = GetDirShadowBias();
	float shadowMapDepth = texture(u_ShadowMap, vec3(shadowCoords.xy * 0.5f + 0.5f, cascade)).x;
	return step(shadowCoords.z, shadowMapDepth + bias) * g_ShadowFade;
}

// Soft Shadows (PCSS)

// Penumbra
// This search area estimation comes from the following article: 
// http://developer.download.nvidia.com/whitepapers/2008/PCSS_Integration.pdf
float SearchWidth(float uvLightSize, float receiverDistance)
{
	const float NEAR = 0.1f;
	return uvLightSize * (receiverDistance - NEAR) / u_Scene.CameraPosition.z;
}

float SearchRegionRadiusUV(float zWorld)
{
	const float light_zNear = 0.01f; // 0.01 gives artifacts? maybe because of ortho proj?
	const float lightRadiusUV = 0.05f;
	return lightRadiusUV * (zWorld - light_zNear) / zWorld;
}

const vec2 PoissonDistribution[64] = vec2[](
	vec2(-0.94201624f , -0.39906216f),
	vec2( 0.94558609f , -0.76890725f),
	vec2(-0.094184101f, -0.92938870f),
	vec2( 0.34495938f ,  0.29387760f),
	vec2(-0.91588581f ,  0.45771432f),
	vec2(-0.81544232f , -0.87912464f),
	vec2(-0.38277543f ,  0.27676845f),
	vec2( 0.97484398f ,  0.75648379f),
	vec2( 0.44323325f , -0.97511554f),
	vec2( 0.53742981f , -0.47373420f),
	vec2(-0.26496911f , -0.41893023f),
	vec2( 0.79197514f ,  0.19090188f),
	vec2(-0.24188840f ,  0.99706507f),
	vec2(-0.81409955f ,  0.91437590f),
	vec2( 0.19984126f ,  0.78641367f),
	vec2( 0.14383161f , -0.14100790f),
	vec2(-0.413923f   , -0.439757f  ),
	vec2(-0.979153f   , -0.201245f  ),
	vec2(-0.865579f   , -0.288695f  ),
	vec2(-0.243704f   , -0.186378f  ),
	vec2(-0.294920f   , -0.055748f  ),
	vec2(-0.604452f   , -0.544251f  ),
	vec2(-0.418056f   , -0.587679f  ),
	vec2(-0.549156f   , -0.415877f  ),
	vec2(-0.238080f   , -0.611761f  ),
	vec2(-0.267004f   , -0.459702f  ),
	vec2(-0.100006f   , -0.229116f  ),
	vec2(-0.101928f   , -0.380382f  ),
	vec2(-0.681467f   , -0.700773f  ),
	vec2(-0.763488f   , -0.543386f  ),
	vec2(-0.549030f   , -0.750749f  ),
	vec2(-0.809045f   , -0.408738f  ),
	vec2(-0.388134f   , -0.773448f  ),
	vec2(-0.429392f   , -0.894892f  ),
	vec2(-0.131597f   ,  0.065058f  ),
	vec2(-0.275002f   ,  0.102922f  ),
	vec2(-0.106117f   , -0.068327f  ),
	vec2(-0.294586f   , -0.891515f  ),
	vec2(-0.629418f   ,  0.379387f  ),
	vec2(-0.407257f   ,  0.339748f  ),
	vec2( 0.071650f   , -0.384284f  ),
	vec2( 0.022018f   , -0.263793f  ),
	vec2( 0.003879f   , -0.136073f  ),
	vec2(-0.137533f   , -0.767844f  ),
	vec2(-0.050874f   , -0.906068f  ),
	vec2( 0.114133f   , -0.070053f  ),
	vec2( 0.163314f   , -0.217231f  ),
	vec2(-0.100262f   , -0.587992f  ),
	vec2(-0.004942f   ,  0.125368f  ),
	vec2( 0.035302f   , -0.619310f  ),
	vec2( 0.195646f   , -0.459022f  ),
	vec2( 0.303969f   , -0.346362f  ),
	vec2(-0.678118f   ,  0.685099f  ),
	vec2(-0.628418f   ,  0.507978f  ),
	vec2(-0.508473f   ,  0.458753f  ),
	vec2( 0.032134f   , -0.782030f  ),
	vec2( 0.122595f   ,  0.280353f  ),
	vec2(-0.043643f   ,  0.312119f  ),
	vec2( 0.132993f   ,  0.085170f  ),
	vec2(-0.192106f   ,  0.285848f  ),
	vec2( 0.183621f   , -0.713242f  ),
	vec2( 0.265220f   , -0.596716f  ),
	vec2(-0.009628f   , -0.483058f  ),
	vec2(-0.018516f   ,  0.435703f  )
);

const vec2 PoissonDisk[16] = vec2[](
	vec2(-0.94201624f , -0.39906216f),
	vec2( 0.94558609f , -0.76890725f),
	vec2(-0.094184101f, -0.92938870f),
	vec2( 0.34495938f ,  0.29387760f),
	vec2(-0.91588581f ,  0.45771432f),
	vec2(-0.81544232f , -0.87912464f),
	vec2(-0.38277543f ,  0.27676845f),
	vec2( 0.97484398f ,  0.75648379f),
	vec2( 0.44323325f , -0.97511554f),
	vec2( 0.53742981f , -0.47373420f),
	vec2(-0.26496911f , -0.41893023f),
	vec2( 0.79197514f ,  0.19090188f),
	vec2(-0.24188840f ,  0.99706507f),
	vec2(-0.81409955f ,  0.91437590f),
	vec2( 0.19984126f ,  0.78641367f),
	vec2( 0.14383161f , -0.14100790f)
);

vec2 SamplePoisson(int index)
{
	return PoissonDistribution[index % 64];
}

float FindBlockerDistance_DirectionalLight(sampler2DArray shadowMap, uint cascade, vec3 shadowCoords, float uvLightSize)
{
	float bias = GetDirShadowBias();

	const int numBlockerSearchSamples = 64;
	int blockers = 0;
	float avgBlockerDistance = 0.0f;

	float searchWidth = SearchRegionRadiusUV(shadowCoords.z);
	for (int i = 0; i < numBlockerSearchSamples; i++)
	{
		float z = textureLod(shadowMap, vec3((shadowCoords.xy * 0.5f + 0.5f) + SamplePoisson(i) * searchWidth, cascade), 0).r;
		if (z < (shadowCoords.z - bias))
		{
			blockers += 1;
			avgBlockerDistance += z;
		}
	}

	if (blockers > 0)
		return avgBlockerDistance / float(blockers);

	return -1.0f;
}

float PCF_DirectionalLight(sampler2DArray shadowMap, uint cascade, vec3 shadowCoords, float uvRadius)
{
	float bias = GetDirShadowBias();
	const int numPCFSamples = 64;

	float sum = 0.0f;
	for (int i = 0; i < numPCFSamples; i++)
	{
		vec2 offset = SamplePoisson(i) * uvRadius;
		float z = textureLod(shadowMap, vec3((shadowCoords.xy * 0.5f + 0.5f) + offset, cascade), 0).r;
		sum += step(shadowCoords.z - bias, z);
	}

	return sum / numPCFSamples;
}

float PCSS_DirectionalLight(sampler2DArray shadowMap, uint cascade, vec3 shadowCoords, float uvLightSize)
{
	float blockerDistance = FindBlockerDistance_DirectionalLight(shadowMap, cascade, shadowCoords, uvLightSize);
	// No occlusion
	if (blockerDistance == -1.0f)
		return 1.0f;

	float penumbraWidth = (shadowCoords.z - blockerDistance) / blockerDistance;

	float NEAR = 0.01f; // NOTE: Tweakable?
	float uvRadius = penumbraWidth * uvLightSize * NEAR / shadowCoords.z; // NOTE: Do we need the division?
	uvRadius = min(uvRadius, 0.002f);

	return PCF_DirectionalLight(shadowMap, cascade, shadowCoords, uvRadius) * g_ShadowFade;
}

////////////////////////////////////////////////////////////////////////////////////////////
// PBR Functions
////////////////////////////////////////////////////////////////////////////////////////////

vec3 RotateVectorAboutY(float angle, vec3 vec)
{
	angle = radians(angle);
	mat3 rotationMatrix = { vec3( cos(angle), 0.0f, sin(angle)),
							vec3( 0.0f      , 1.0f,       0.0f),
							vec3(-sin(angle), 0.0f, cos(angle)) };
	return rotationMatrix * vec;
}

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2
float NDFGGX(float cosLh, float roughness)
{
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0f) + 1.0f;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float GaSchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0f - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float GaSchlickGGX(float cosLi, float NdotV, float roughness)
{
	float r = roughness + 1.0f;
	float k = (r * r) / 8.0f; // Epic suggests using this roughness remapping for analytic lights.
	return GaSchlickG1(cosLi, k) * GaSchlickG1(NdotV, k);
}

// Shlick's approximation of the Fresnel factor.
vec3 FresnelSchlickRoughness(vec3 F0, float cosTheta, float roughness)
{
	return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 IBL(vec3 F0, vec3 Lr)
{
	vec3 irradiance = texture(u_IrradianceMap, m_Params.Normal).rgb;
	vec3 F = FresnelSchlickRoughness(F0, m_Params.NdotV, m_Params.Roughness);
	vec3 kd = (1.0f - F) * (1.0f - m_Params.Metalness);
	vec3 diffuseIBL = m_Params.Albedo * irradiance;

	int envRadianceTexLevels = textureQueryLevels(u_RadianceMap);
	vec3 specularIrradiance = textureLod(u_RadianceMap, RotateVectorAboutY(u_MaterialUniforms.EnvMapRotation, Lr), m_Params.Roughness * envRadianceTexLevels).rgb;

	vec2 specularBRDF = texture(u_BRDFLutTexture, vec2(m_Params.NdotV, m_Params.Roughness)).rg;
	vec3 specularIBL = specularIrradiance * (F0 * specularBRDF.r + specularBRDF.g);

	return kd * diffuseIBL + specularIBL;
}

vec3 CalculateDirectionalLight(vec3 F0)
{
	vec3 result = vec3(0.0f);

	// Means we the effect of the directional light is nothing
	if (u_Scene.DirLight.Radiance.a == 0.0f)
		return result;

	vec3 Li = u_Scene.DirLight.Direction.xyz;
	vec3 Lradiance = u_Scene.DirLight.Radiance.rgb * u_Scene.DirLight.Radiance.a; // Alpha channel is the light multiplier
	vec3 Lh = normalize(Li + m_Params.View);

	// Calculate angles between surface normal and various light vectors
	float cosLi = max(0.0f, dot(m_Params.Normal, Li));
	float cosLh = max(0.0f, dot(m_Params.Normal, Lh));

	vec3 F = FresnelSchlickRoughness(F0, max(0.0f, dot(Lh, m_Params.View)), m_Params.Roughness);
	float D = NDFGGX(cosLh, m_Params.Roughness);
	float G = GaSchlickGGX(cosLi, m_Params.NdotV, m_Params.Roughness);

	vec3 kd = (1.0f - F) * (1.0f - m_Params.Metalness);
	vec3 diffuseBRDF = kd * m_Params.Albedo;

	// Cook-Torrance
	vec3 specularBRDF = (F * D * G) / max(Epsilon, 4.0f * cosLi * m_Params.NdotV);
	specularBRDF = clamp(specularBRDF, vec3(0.0f), vec3(10.0f));
	result += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;

	return result;
}

int GetPointLightBufferIndex(int i)
{
	ivec2 tileID = ivec2(gl_FragCoord) / ivec2(IR_LIGHT_CULLING_WORKGROUP_SIZE);
	uint index = tileID.y * u_RendererData.TilesCountX + tileID.x;

	uint offset = index * IR_MAX_POINT_LIGHT_COUNT;
	return s_VisiblePointLightIndicesBuffer.Indices[offset + i];
}

int GetPointLightCount()
{
	int result = 0;
	for (int i = 0; i < u_PointLights.LightCount; i++)
	{
		uint lightIndex = GetPointLightBufferIndex(i);
		if (lightIndex == -1)
			break;

		result++;
	}

	return result;
}

vec3 CalculatePointLights(in vec3 F0, vec3 worldPos)
{
	vec3 result = vec3(0.0);
	for (int i = 0; i < u_PointLights.LightCount; i++)
	{
		uint lightIndex = GetPointLightBufferIndex(i);
		if (lightIndex == -1)
			break;

		PointLight light = u_PointLights.Lights[lightIndex];
		vec3 Li = normalize(light.Position - worldPos);
		float lightDistance = length(light.Position - worldPos);
		vec3 Lh = normalize(Li + m_Params.View);

		float attenuation = clamp(1.0 - (lightDistance * lightDistance) / (light.Radius * light.Radius), 0.0, 1.0);
		attenuation *= mix(attenuation, 1.0, light.Falloff);

		vec3 Lradiance = light.Radiance * light.Intensity * attenuation;

		// Calculate angles between surface normal and various light vectors.
		float cosLi = max(0.0, dot(m_Params.Normal, Li));
		float cosLh = max(0.0, dot(m_Params.Normal, Lh));

		vec3 F = FresnelSchlickRoughness(F0, max(0.0, dot(Lh, m_Params.View)), m_Params.Roughness);
		float D = NDFGGX(cosLh, m_Params.Roughness);
		float G = GaSchlickGGX(cosLi, m_Params.NdotV, m_Params.Roughness);

		vec3 kd = (1.0 - F) * (1.0 - m_Params.Metalness);
		vec3 diffuseBRDF = kd * m_Params.Albedo;

		// Cook-Torrance
		vec3 specularBRDF = (F * D * G) / max(Epsilon, 4.0 * cosLi * m_Params.NdotV);
		specularBRDF = clamp(specularBRDF, vec3(0.0f), vec3(10.0f));
		result += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;
	}

	return result;
}

int GetSpotLightBufferIndex(int i)
{
	ivec2 tileID = ivec2(gl_FragCoord) / ivec2(IR_LIGHT_CULLING_WORKGROUP_SIZE);
	uint index = tileID.y * u_RendererData.TilesCountX + tileID.x;

	uint offset = index * IR_MAX_SPOT_LIGHT_COUNT;
	return s_VisibleSpotLightIndicesBuffer.Indices[offset + i];
}


int GetSpotLightCount()
{
	int result = 0;
	for (int i = 0; i < u_SpotLights.LightCount; i++)
	{
		uint lightIndex = GetSpotLightBufferIndex(i);
		if (lightIndex == -1)
			break;

		result++;
	}

	return result;
}

vec3 CalculateSpotLights(in vec3 F0, vec3 worldPos)
{
	vec3 result = vec3(0.0f);
	for (int i = 0; i < u_SpotLights.LightCount; i++)
	{
		uint lightIndex = GetSpotLightBufferIndex(i); 
		if (lightIndex == -1)
			break;

		SpotLight light = u_SpotLights.Lights[lightIndex];
		vec3 Li = normalize(light.Position - worldPos);
		float lightDistance = length(light.Position - worldPos);

		float cutoff = cos(radians(light.Angle * 0.5f));
		float scos = max(dot(Li, light.Direction), cutoff);
		float rim = (1.0f - scos) / (1.0f - cutoff);

		float attenuation = clamp(1.0f - (lightDistance * lightDistance) / (light.Range * light.Range), 0.0f, 1.0f);
		attenuation *= mix(attenuation, 1.0f, light.Falloff);
		attenuation *= 1.0f - pow(max(rim, 0.001f), light.AngleAttenuation);

		vec3 Lradiance = light.Radiance * light.Intensity * attenuation;
		vec3 Lh = normalize(Li + m_Params.View);

		// Calculate angles between surface normal and various light vectors.
		float cosLi = max(0.0f, dot(m_Params.Normal, Li));
		float cosLh = max(0.0f, dot(m_Params.Normal, Lh));

		vec3 F = FresnelSchlickRoughness(F0, max(0.0f, dot(Lh, m_Params.View)), m_Params.Roughness);
		float D = NDFGGX(cosLh, m_Params.Roughness);
		float G = GaSchlickGGX(cosLi, m_Params.NdotV, m_Params.Roughness);

		vec3 kd = (1.0f - F) * (1.0f - m_Params.Metalness);
		vec3 diffuseBRDF = kd * m_Params.Albedo;

		// Cook-Torrance
		vec3 specularBRDF = (F * D * G) / max(Epsilon, 4.0f * cosLi * m_Params.NdotV);
		specularBRDF = clamp(specularBRDF, vec3(0.0f), vec3(10.0f));
		result += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;

	}

	return result;
}

vec4 ToLinear(vec4 sRGB)
{
	bvec4 cutoff = lessThan(sRGB, vec4(0.04045f));
	vec4 higher = pow((sRGB + vec4(0.055f)) / vec4(1.055f), vec4(2.4f));
	vec4 lower = sRGB / vec4(12.92f);

	return mix(higher, lower, cutoff);
}

vec3 GetGradient(float value)
{
	vec3 zero	 = vec3(0.0f, 0.0f, 0.0f);
	vec3 white	 = vec3(0.0f, 0.1f, 0.9f);
	vec3 red	 = vec3(0.2f, 0.9f, 0.4f);
	vec3 blue	 = vec3(0.8f, 0.8f, 0.3f);
	vec3 green	 = vec3(0.9f, 0.2f, 0.3f);

	float step0 = 0.0f;
	float step1 = 2.0f;
	float step2 = 4.0f;
	float step3 = 8.0f;
	float step4 = 16.0f;

	vec3 color = mix(zero, white, smoothstep(step0, step1, value));
	color = mix(color, white, smoothstep(step1, step2, value));
	color = mix(color, red, smoothstep(step1, step2, value));
	color = mix(color, blue, smoothstep(step2, step3, value));
	color = mix(color, green, smoothstep(step3, step4, value));

	return color;
}

void main()
{
	uint cascadeIndex = 0;

	if (!u_RendererData.Unlit)
	{
		vec4 albedoTexColor = texture(u_AlbedoTexture, Input.TexCoord * u_MaterialUniforms.Tiling);
		m_Params.Albedo = albedoTexColor.rgb * ToLinear(vec4(u_MaterialUniforms.AlbedoColor, 1.0f)).rgb; // u_MaterialUniforms.AlbedoColor is perceptual, must be converted to linear

		// note: Metalness and roughness could be in the same texture.
		//       Per GLTF spec, we read metalness from the B channel and roughness from the G channel
		//       This will still work if metalness and roughness are independent greyscale textures,
		//       but it will not work if metalness and roughness are independent textures containing only R channel.
		m_Params.Metalness = texture(u_MetalnessTexture, Input.TexCoord * u_MaterialUniforms.Tiling).b * u_MaterialUniforms.Metalness;
		m_Params.Roughness = texture(u_RoughnessTexture, Input.TexCoord * u_MaterialUniforms.Tiling).g * u_MaterialUniforms.Roughness;
		// Write final metalness roughness values
		o_MetalnessRoughness = vec4(m_Params.Metalness, m_Params.Roughness, 0.0f, 1.0f);
		m_Params.Roughness = max(m_Params.Roughness, 0.05f); // Min roughness value above 0 so we keep specular highlights

		// Normals... Either from vertex data or from normal map
		m_Params.Normal = normalize(Input.Normal);
		if (u_MaterialUniforms.UseNormalMap)
		{
			m_Params.Normal = normalize(texture(u_NormalTexture, Input.TexCoord * u_MaterialUniforms.Tiling).rgb * 2.0f - 1.0f);
			m_Params.Normal = normalize(Input.WorldNormals * m_Params.Normal);
		}

		// Write final view normals
		o_ViewNormalsLuminance.rgb = Input.CameraView * m_Params.Normal;

		m_Params.View = normalize(u_Scene.CameraPosition - Input.WorldPosition);
		m_Params.NdotV = max(dot(m_Params.Normal, m_Params.View), 0.0f);

		// Specular reflection vector
		vec3 Lr = 2.0f * m_Params.NdotV * m_Params.Normal - m_Params.View;

		// Fresnel reflectance, metals use albedo
		vec3 F0 = mix(c_Fdielectric, m_Params.Albedo, m_Params.Metalness);

		const uint SHADOW_MAP_CASCADE_COUNT = 4;
		for (uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; i++)
		{
			if (Input.ViewPosition.z < u_RendererData.CascadeSplits[i])
				cascadeIndex = i + 1;
		}

		float distance = length(Input.ViewPosition);
		g_ShadowFade = distance - (u_RendererData.MaxShadowDistance - u_RendererData.ShadowFade);
		g_ShadowFade /= u_RendererData.ShadowFade;
		g_ShadowFade = clamp(1.0f - g_ShadowFade, 0.0f, 1.0f);

		float shadowScale;
		if (u_RendererData.CascadeFading)
		{
			float cascadeTransitionFade = u_RendererData.CascadeTransitionFade;

			float c0 = smoothstep(u_RendererData.CascadeSplits[0] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[0] - cascadeTransitionFade * 0.5f, Input.ViewPosition.z);
			float c1 = smoothstep(u_RendererData.CascadeSplits[1] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[1] - cascadeTransitionFade * 0.5f, Input.ViewPosition.z);
			float c2 = smoothstep(u_RendererData.CascadeSplits[2] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[2] - cascadeTransitionFade * 0.5f, Input.ViewPosition.z);

			if (c0 > 0.0f && c0 < 1.0f)
			{
				// Sample cascades 0 and 1
				vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 0);
				float shadowAmount0 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMap, 0, shadowMapCoords, u_Scene.DirLight.LightSize) : HardShadows_DirectionalLight(u_ShadowMap, 0, shadowMapCoords);
				shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 1);
				float shadowAmount1 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMap, 1, shadowMapCoords, u_Scene.DirLight.LightSize) : HardShadows_DirectionalLight(u_ShadowMap, 1, shadowMapCoords);

				shadowScale = mix(shadowAmount0, shadowAmount1, c0);
			}
			else if (c1 > 0.0f && c1 < 1.0f)
			{
				// Sample cascades 1 and 2
				vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 1);
				float shadowAmount1 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMap, 1, shadowMapCoords, u_Scene.DirLight.LightSize) : HardShadows_DirectionalLight(u_ShadowMap, 1, shadowMapCoords);
				shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 2);
				float shadowAmount2 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMap, 2, shadowMapCoords, u_Scene.DirLight.LightSize) : HardShadows_DirectionalLight(u_ShadowMap, 2, shadowMapCoords);

				shadowScale = mix(shadowAmount1, shadowAmount2, c1);
			}
			else if (c2 > 0.0f && c2 < 1.0f)
			{
				// Sample cascades 2 and 3
				vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 2);
				float shadowAmount2 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMap, 2, shadowMapCoords, u_Scene.DirLight.LightSize) : HardShadows_DirectionalLight(u_ShadowMap, 2, shadowMapCoords);
				shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 3);
				float shadowAmount3 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMap, 3, shadowMapCoords, u_Scene.DirLight.LightSize) : HardShadows_DirectionalLight(u_ShadowMap, 3, shadowMapCoords);

				shadowScale = mix(shadowAmount2, shadowAmount3, c2);
			}
			else
			{
				vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, cascadeIndex);
				shadowScale = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMap, cascadeIndex, shadowMapCoords, u_Scene.DirLight.LightSize) : HardShadows_DirectionalLight(u_ShadowMap, cascadeIndex, shadowMapCoords);
			}
		}
		else
		{
			vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, cascadeIndex);
			shadowScale = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMap, cascadeIndex, shadowMapCoords, u_Scene.DirLight.LightSize) : HardShadows_DirectionalLight(u_ShadowMap, cascadeIndex, shadowMapCoords);
		}

		shadowScale = 1.0f - clamp(u_Scene.DirLight.Direction.a - shadowScale, 0.0f, 1.0f);

		// Indirect Lighting
		vec3 iblContribution = IBL(F0, Lr) * u_Scene.EnvironmentMapIntensity;

		// Direct Lighting
		vec3 directLightingContrib = CalculateDirectionalLight(F0) * shadowScale;
		directLightingContrib += CalculatePointLights(F0, Input.WorldPosition);
		directLightingContrib += CalculateSpotLights(F0, Input.WorldPosition);
		directLightingContrib += m_Params.Albedo * u_MaterialUniforms.Emission; // For bloom filtering pass

		// Write final color
		o_Color = vec4(iblContribution + directLightingContrib, 1.0f);

		// TODO: Temp hacky fix
		if (u_Scene.DirLight.Radiance.a <= 0.0f)
			shadowScale = 0.0f;

		// Shadow mask with respect to bright surfaces
		o_ViewNormalsLuminance.a = clamp(shadowScale + dot(o_Color.rgb, vec3(0.2125f, 0.7154f, 0.0721f)), 0.0f, 1.0f);
		
		// (shading-only)
		// o_Color.rgb = vec3(1.0) * shadowScale + 0.2f;
	}
	else
	{
		// Applies a bit of dimming in all the channels for the unlit version otherwise its too bright
		o_Color =  vec4(vec3(0.7f), 1.0f) * texture(u_AlbedoTexture, Input.TexCoord * u_MaterialUniforms.Tiling) * vec4(u_MaterialUniforms.AlbedoColor, 1.0f);
		o_ViewNormalsLuminance = vec4(Input.CameraView * normalize(Input.Normal), 1.0f);
		float metalness = texture(u_MetalnessTexture, Input.TexCoord * u_MaterialUniforms.Tiling).b * u_MaterialUniforms.Metalness;
		float roughness = texture(u_RoughnessTexture, Input.TexCoord * u_MaterialUniforms.Tiling).g * u_MaterialUniforms.Roughness;
		o_MetalnessRoughness = vec4(metalness, roughness, 0.0f, 1.0f);
	}

	if (u_RendererData.ShowLightComplexity)
	{
		int pointLightCount = GetPointLightCount();
		int spotLightCount = GetSpotLightCount();

		float value = float(pointLightCount + spotLightCount);
		o_Color.rgb = (o_Color.rgb * 0.2f) + GetGradient(value);
	}

	if (u_RendererData.ShowCascades)
	{
		switch (cascadeIndex)
		{
			case 0:
				o_Color.rgb *= vec3(1.0f, 0.25f, 0.25f);
				break;
			case 1:
				o_Color.rgb *= vec3(0.25f, 1.0f, 0.25f);
				break;
			case 2:
				o_Color.rgb *= vec3(0.25f, 0.25f, 1.0f);
				break;
			case 3:
				o_Color.rgb *= vec3(1.0f, 1.0f, 0.25f);
				break;
		}
	}
}