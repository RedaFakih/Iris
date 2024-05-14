#version 450 core
#stage compute

// Converts equirectangular (lat-long) projection texture into a proper cubemap.
// Reference: Michal Siejak's PBR project (https://github.com/Nadrin)

const float PI = 3.14159265358979323846f;
const float TwoPI = 2 * PI;

// This is handled by the renderpass
layout(set = 2, binding = 0, rgba16f) restrict writeonly uniform imageCube o_OutputCubeMap;
// Handled by the material
layout(set = 3, binding = 0) uniform sampler2D u_EquirectangularTexture;

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

// Specify the size of invocations per workgroup (size of workgroups was specified on C++ land via glDispatchCompute(x, y, z))
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

void main()
{
	vec3 cubeTC = GetCubeMapTexCoord(vec2(imageSize(o_OutputCubeMap)));

	// Convert Cartesian direction vector to spherical coordinates.
	// Calculate sampling coords for equirectangular texture
	// https://en.wikipedia.org/wiki/Spherical_coordinate_system#Cartesian_coordinates
	float phi = atan(cubeTC.z, cubeTC.x);
	float theta = acos(cubeTC.y);
	
    vec2 texCoord = vec2(phi / (2.0f * PI) + 0.5f, theta / PI);

	// Sample equirectangular texture.
	vec4 color = texture(u_EquirectangularTexture, texCoord);

	// Write out color into the cubemap;
	imageStore(o_OutputCubeMap, ivec3(gl_GlobalInvocationID), color);
}