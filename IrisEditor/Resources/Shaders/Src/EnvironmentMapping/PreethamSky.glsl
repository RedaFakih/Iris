#version 450 core
#stage compute

// Preetham Sky shader
// TODO: Need to look into more of with this adaptation
// Heavily adapted from https://www.shadertoy.com/view/llSSDR

const float PI = 3.14159265358979323846f;

layout(set = 3, binding = 0, rgba16f) restrict writeonly uniform imageCube o_OutputCubeMap;

layout(push_constant) uniform Uniforms
{
	vec4 TurbidityAzimuthInclinationSunSize;
} u_Uniforms;

// So we get a 3D vector that points to a pixel inside on one of the cubemap faces based on the GlobalInvocationID inside
// a workgroup. Basically we go from a 2D equirectangular picture to a 3D cubemap.
// Calculate normalized sampling direction vector based on current fragment coordinates (gl_GlobalInvocationID.xyz).
// This is essentially "inverse-sampling": we reconstruct what the sampling vector would be if we wanted it to "hit"
// this particular fragment in a cubemap.
// See: OpenGL core profile specs, section 8.13.
vec3 GetCubeMapTexCoord(vec2 outputImageSize)
{
    vec2 st = gl_GlobalInvocationID.xy / outputImageSize;
    vec2 uv = 2.0f * vec2(st.x, 1.0f - st.y) - vec2(1.0f);

    vec3 ret;
    if (gl_GlobalInvocationID.z == 0u)      ret = vec3( 1.0f,  uv.y, -uv.x);
    else if (gl_GlobalInvocationID.z == 1u) ret = vec3(-1.0f,  uv.y,  uv.x);
    else if (gl_GlobalInvocationID.z == 2u) ret = vec3( uv.x,  1.0f, -uv.y);
    else if (gl_GlobalInvocationID.z == 3u) ret = vec3( uv.x, -1.0f,  uv.y);
    else if (gl_GlobalInvocationID.z == 4u) ret = vec3( uv.x,  uv.y,  1.0f);
    else if (gl_GlobalInvocationID.z == 5u) ret = vec3(-uv.x,  uv.y, -1.0f);

    return normalize(ret);
}

float SaturatedDot(vec3 a, vec3 b)
{
	return max(dot(a, b), 0.0f);   
}

vec3 YxyToXYZ(vec3 Yxy)
{
	float Y = Yxy.r;
	float x = Yxy.g;
	float y = Yxy.b;

	float X = x * (Y / y);
	float Z = (1.0f - x - y) * (Y / y);

	return vec3(X, Y, Z);
}

vec3 XYZToRGB(vec3 XYZ)
{
	// CIE/E
	mat3 M = mat3
	(
		 2.3706743f, -0.9000405f, -0.4706338f,
		-0.5138850f,  1.4253036f,  0.0885814f,
 		 0.0052982f, -0.0146949f,  1.0093968f
	);

	return XYZ * M;
}

vec3 YxyToRGB(vec3 Yxy)
{
	vec3 XYZ = YxyToXYZ(Yxy);
	return XYZToRGB(XYZ);
}

void CalculatePerezDistribution(float t, out vec3 A, out vec3 B, out vec3 C, out vec3 D, out vec3 E)
{
	A = vec3( 0.1787f * t - 1.4630f, -0.0193f * t - 0.2592f, -0.0167f * t - 0.2608f);
	B = vec3(-0.3554f * t + 0.4275f, -0.0665f * t + 0.0008f, -0.0950f * t + 0.0092f);
	C = vec3(-0.0227f * t + 5.3251f, -0.0004f * t + 0.2125f, -0.0079f * t + 0.2102f);
	D = vec3( 0.1206f * t - 2.5771f, -0.0641f * t - 0.8989f, -0.0441f * t - 1.6537f);
	E = vec3(-0.0670f * t + 0.3703f, -0.0033f * t + 0.0452f, -0.0109f * t + 0.0529f);
}

vec3 CalculateZenithLuminanceYxy(float t, float thetaS)
{
	float chi  	 	= (4.0f / 9.0f - t / 120.0f) * (PI - 2.0f * thetaS);
	float Yz   	 	= (4.0453f * t - 4.9710f) * tan(chi) - 0.2155f * t + 2.4192f;

	float theta2 	= thetaS * thetaS;
    float theta3 	= theta2 * thetaS;
    float T 	 	= t;
    float T2 	 	= t * t;

	float xz =
      ( 0.00165f * theta3 - 0.00375f * theta2 + 0.00209f * thetaS + 0.0f)     * T2 +
      (-0.02903f * theta3 + 0.06377f * theta2 - 0.03202f * thetaS + 0.00394f) * T +
      ( 0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * thetaS + 0.25886f);

    float yz =
      ( 0.00275f * theta3 - 0.00610f * theta2 + 0.00317f * thetaS + 0.0f)     * T2 +
      (-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * thetaS + 0.00516f) * T +
      ( 0.15346f * theta3 - 0.26756f * theta2 + 0.06670f * thetaS + 0.26688f);

	return vec3(Yz, xz, yz);
}

vec3 CalculatePerezLuminanceYxy(float theta, float gamma, vec3 A, vec3 B, vec3 C, vec3 D, vec3 E)
{
	return (1.0f + A * exp(B / cos(theta))) * (1.0f + C * exp(D * gamma) + E * cos(gamma) * cos(gamma));
}

vec3 CalculateSkyLuminanceRGB(vec3 s, vec3 e, float t)
{
	vec3 A, B, C, D, E;
	CalculatePerezDistribution(t, A, B, C, D, E);

	float thetaS = acos(SaturatedDot(s, vec3(0.0f, 1.0f, 0.0f)));
	float thetaE = acos(SaturatedDot(e, vec3(0.0f, 1.0f, 0.0f)));
	float gammaE = acos(SaturatedDot(s, e));

	vec3 Yz = CalculateZenithLuminanceYxy(t, thetaS);

	vec3 fThetaGamma = CalculatePerezLuminanceYxy(thetaE, gammaE, A, B, C, D, E);
	vec3 fZeroThetaS = CalculatePerezLuminanceYxy(0.0f,   thetaS, A, B, C, D, E);

	vec3 Yp = Yz * (fThetaGamma / fZeroThetaS);

	return YxyToRGB(Yp);
}

vec3 CalculateSunColor(float inclination)
{
    const vec3 horizonColor = vec3(1.0f, 0.5f, 0.0f); // Reddish color at the horizon
    const vec3 zenithColor = vec3(1.0f, 1.0f, 0.9f); // Whitish color at the zenith
    float t = cos(inclination); // Use cosine to smooth the transition
    return mix(horizonColor, zenithColor, t - 0.02f);
}

vec3 CalculateSunContribution(vec3 sunDir, vec3 viewDir, vec3 sunColor, float sunSize, float inclination)
{
	float distanceSquared = dot(viewDir - sunDir, viewDir - sunDir);
	float denom = 2.0f * sunSize * sunSize;
    float sunIntensity = exp(-distanceSquared / (denom));
	float coreIntensity = exp(-distanceSquared / (denom * 0.2f));
	sunIntensity += coreIntensity;

    return sunColor * sunIntensity;
}

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main()
{
	vec3 cubeTC = GetCubeMapTexCoord(vec2(imageSize(o_OutputCubeMap)));

	float turbidity     = u_Uniforms.TurbidityAzimuthInclinationSunSize.x;
    float azimuth       = u_Uniforms.TurbidityAzimuthInclinationSunSize.y;
    float inclination   = u_Uniforms.TurbidityAzimuthInclinationSunSize.z;
	float sunSize       = u_Uniforms.TurbidityAzimuthInclinationSunSize.w;
    vec3 sunDir     	= normalize(vec3(sin(inclination) * cos(azimuth), cos(inclination), sin(inclination) * sin(azimuth)));
    vec3 viewDir  		= cubeTC;
    vec3 skyLuminance 	= CalculateSkyLuminanceRGB(sunDir, viewDir, turbidity);

	// Sun
	vec3 sunContribution = CalculateSunContribution(sunDir, viewDir, CalculateSunColor(inclination), sunSize, inclination);

    vec4 color = vec4(skyLuminance * 0.05f + sunContribution, 1.0f);
	imageStore(o_OutputCubeMap, ivec3(gl_GlobalInvocationID), color);
}