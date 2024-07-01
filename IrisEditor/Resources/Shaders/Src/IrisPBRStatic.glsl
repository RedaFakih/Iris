#version 450 core
#stage vertex

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

struct VertexOutput
{
	vec3 WorldPosition;
	vec3 Normal;
	vec2 TexCoord;
	mat3 WorldNormals;
	mat3 WorldTransform;
	vec3 Binormal;

	mat3 CameraView;
	vec3 ViewPosition;
};

layout(location = 0) out VertexOutput Output;

// Make sure both the PreDdepth shader and the PBR shader compute the exact same result
invariant gl_Position;

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

	mat3 CameraView;
	vec3 ViewPosition;
};

layout(location = 0) in VertexOutput Input;

layout(set = 0, binding = 0) uniform sampler2D u_BRDFLutTexture;

layout(set = 2, binding = 0) uniform samplerCube u_RadianceMap;
layout(set = 2, binding = 1) uniform samplerCube u_IrradianceMap;

layout(set = 3, binding = 0) uniform sampler2D u_AlbedoTexture;
layout(set = 3, binding = 1) uniform sampler2D u_NormalTexture;
layout(set = 3, binding = 2) uniform sampler2D u_RoughnessTexture;
layout(set = 3, binding = 3) uniform sampler2D u_MetalnessTexture;

// Set = 1 has uniform buffers
struct DirectionalLight
{
	vec4 Direction; // The alpha channel is the ShadowAmount
	vec4 Radiance; // The alpha channel is the multiplier of the directional light
};

layout(std140, set = 1, binding = 2) uniform SceneData
{
	DirectionalLight DirLight;
	vec3 CameraPosition; // Offset 32
	float EnvironmentMapIntensity; // This is used in the PBR shader and it mirrors the intensity that is used in the Skybox shader
} u_Scene;

layout(push_constant) uniform Materials
{
    vec3 AlbedoColor;
    float Roughness;
    float Metalness;
    float Emission;

	float EnvMapRotation;

    bool UseNormalMap;
	bool Lit;
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

vec4 ToLinear(vec4 sRGB)
{
	bvec4 cutoff = lessThan(sRGB, vec4(0.04045f));
	vec4 higher = pow((sRGB + vec4(0.055f)) / vec4(1.055f), vec4(2.4f));
	vec4 lower = sRGB / vec4(12.92f);

	return mix(higher, lower, cutoff);
}

void main()
{
	if (u_MaterialUniforms.Lit)
	{
		vec4 albedoTexColor = texture(u_AlbedoTexture, Input.TexCoord);
		m_Params.Albedo = albedoTexColor.rgb * ToLinear(vec4(u_MaterialUniforms.AlbedoColor, 1.0f)).rgb; // u_MaterialUniforms.AlbedoColor is perceptual, must be converted to linear

		// note: Metalness and roughness could be in the same texture.
		//       Per GLTF spec, we read metalness from the B channel and roughness from the G channel
		//       This will still work if metalness and roughness are independent greyscale textures,
		//       but it will not work if metalness and roughness are independent textures containing only R channel.
		m_Params.Metalness = texture(u_MetalnessTexture, Input.TexCoord).b * u_MaterialUniforms.Metalness;
		m_Params.Roughness = texture(u_RoughnessTexture, Input.TexCoord).g * u_MaterialUniforms.Roughness;
		// Write final metalness roughness values
		o_MetalnessRoughness = vec4(m_Params.Metalness, m_Params.Roughness, 0.0f, 1.0f);
		m_Params.Roughness = max(m_Params.Roughness, 0.05f); // Min roughness value above 0 so we keep specular highlights

		// Normals... Either from vertex data or from normal map
		m_Params.Normal = normalize(Input.Normal);
		if (u_MaterialUniforms.UseNormalMap)
		{
			m_Params.Normal = normalize(texture(u_NormalTexture, Input.TexCoord).rgb * 2.0f - 1.0f);
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

		// Indirect Lighting
		vec3 iblContribution = IBL(F0, Lr) * u_Scene.EnvironmentMapIntensity;

		// Direct Lighting
		vec3 directLightingContrib = CalculateDirectionalLight(F0);
		directLightingContrib += m_Params.Albedo * u_MaterialUniforms.Emission; // For bloom filtering pass

		// Write final color
		o_Color = vec4(iblContribution + directLightingContrib, 1.0f);

		o_ViewNormalsLuminance.a = clamp(/* shadowScale + */dot(o_Color.rgb, vec3(0.2125f, 0.7154f, 0.0721f)), 0.0f, 1.0f); // Shadow mask with respect to bright surfaces
	}
	else
	{
		// Applies a bit of dimming in all the channels for the unlit version otherwise its too bright
		o_Color =  vec4(vec3(0.6f), 1.0f) * texture(u_AlbedoTexture, Input.TexCoord) * vec4(u_MaterialUniforms.AlbedoColor, 1.0f);
		o_ViewNormalsLuminance = vec4(Input.CameraView * normalize(Input.Normal), 1.0f);
		float metalness = texture(u_MetalnessTexture, Input.TexCoord).b * u_MaterialUniforms.Metalness;
		float roughness = texture(u_RoughnessTexture, Input.TexCoord).g * u_MaterialUniforms.Roughness;
		o_MetalnessRoughness = vec4(metalness, roughness, 0.0f, 1.0f);
	}
}