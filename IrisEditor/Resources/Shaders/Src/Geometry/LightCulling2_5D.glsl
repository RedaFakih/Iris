#version 450 core
#stage compute
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require

// Adapted from: https://wickedengine.net/

// The original was ported from WickedEngine, which is D3D (left handed, +Z forward, texture v=0 at NDC y=+1).
// Iris is Vulkan + glm right handed (-Z forward), no projection Y flip and a positive height viewport for offscreen passes.
// The fixes, tagged in the code below:
//
//  [FIX 1] Tile size. C++ dispatches ceil(viewport / IR_LIGHT_CULLING_WORKGROUP_SIZE) groups and IrisPBRStatic.glsl reads
//          tile = gl_FragCoord / IR_LIGHT_CULLING_WORKGROUP_SIZE, so one tile MUST be 8x8 pixels. The original treated
//          every group as a 16x16 pixel tile (8x8 threads * 2x2 samples, TILED_CULLING_BLOCKSIZE 16).
//  [FIX 2] No "1 - uv.y" in ScreenToView. In Iris texel row 0 == NDC y = -1. With the flip the tile frusta were mirrored
//          vertically AND (together with the handedness change) the side plane normals pointed OUTWARD.
//  [FIX 3] Right handed view space. Every depth comparison is now done on the POSITIVE view depth (-viewPos.z).
//          The original near/far test (sphere.c.z + r < zNear) rejected basically every light in front of the camera
//          and ConstructEntityMask got start > end, so the uint shift amount underflowed (undefined behaviour).
//  [FIX 4] Spot lights. light.Angle is the FULL cone angle in DEGREES (see CalculateSpotLights in IrisPBRStatic.glsl),
//          the original did cos(light.Angle) (radians, half angle). The bounding sphere now encloses the region the PBR
//          shader actually lights (a spherical sector: radius Range, half angle Angle / 2) for every angle in [0.1, 180].
//  [FIX 5] Robustness: bounds checks on the shared index arrays, guarded depth range reciprocal, side planes that can not
//          degenerate when the tile's min depth == max depth, projection agnostic depth linearization (works for ortho
//          too), subgroupElect()
// =====================================================================================================================

// References:
// - SIGGRAPH 2011 - Rendering in Battlefield 3
// - https://wickedengine.net/2018/01/optimizing-tile-based-light-culling/

// [FIX 1] One tile == IR_LIGHT_CULLING_WORKGROUP_SIZE * TILED_CULLING_GRANULARITY pixels.
// GRANULARITY MUST stay 1 unless ALSO changing:
//   - SceneRenderer::BeginScene() (m_NeedsResize block): m_LightCullingWorkGroups = ceil(viewport / TILED_CULLING_BLOCKSIZE)
//   - IrisPBRStatic.glsl: tileID = ivec2(gl_FragCoord) / TILED_CULLING_BLOCKSIZE
#define TILED_CULLING_BLOCKSIZE (IR_LIGHT_CULLING_WORKGROUP_SIZE * IR_LIGHT_CULLING_GRANULARITY)

layout(set = 2, binding = 0) uniform sampler2D u_DepthMap;

layout(std140, set = 1, binding = 0) uniform Camera
{
	mat4 ViewProjectionMatrix;
	mat4 InverseViewProjectionMatrix;
	mat4 ProjectionMatrix;
	mat4 InverseProjectionMatrix;
	mat4 ViewMatrix;
	mat4 InverseViewMatrix;
	vec2 NDCToViewMul;
	vec2 NDCToViewAdd;
	vec2 DepthUnpackConsts;
} u_Camera;

layout(std140, set = 1, binding = 1) uniform ScreenData
{
	vec2 FullResolution;
	vec2 InverseFullResolution;
	vec2 HalfResolution;
	vec2 InverseHalfResolution;
} u_ScreenData;

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

layout(std430, set = 1, binding = 5) writeonly buffer VisiblePointLightIndicesBuffer
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper Functions and structures
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const uint c_MaxPointLights = uint(IR_MAX_POINT_LIGHT_COUNT);
const uint c_MaxSpotLights = uint(IR_MAX_SPOT_LIGHT_COUNT);

struct Plane
{
	vec3  N; // Plane normal (points INTO the tile frustum)
	float d;
};

Plane ComputePlane(vec3 p0, vec3 p1, vec3 p2)
{
	Plane plane;
	plane.N = normalize(cross(p1 - p0, p2 - p0));
	plane.d = dot(plane.N, p0);
	return plane;
}

// The four side planes of a tile frustum, in view space. Near / far are handled with the tile depth bounds.
struct Frustum
{
	Plane Planes[4];
};

struct Sphere
{
	vec3 c;
	float r;
};

struct AABB
{
	vec3 c;
	vec3 e;
};

vec4 ClipToView(vec4 clip)
{
	vec4 view = u_Camera.InverseProjectionMatrix * clip;
	return view / view.w;
}

// [FIX 2] Iris: positive height viewport + glm projection without Y flip => texel row 0 is NDC y = -1, so there is NO
// "1 - texCoord.y" here (that is the D3D convention WickedEngine uses).
vec4 ScreenToView(vec4 screen)
{
	vec2 texCoord = screen.xy * u_ScreenData.InverseFullResolution;
	vec4 clip = vec4(texCoord * 2.0f - 1.0f, screen.z, screen.w);
	return ClipToView(clip);
}

// [FIX 3] Returns the POSITIVE distance along the view direction (Iris view space is right handed, looking down -Z).
// Only z and w of InverseProjection * vec4(x, y, depth, 1) are needed and they do not depend on x / y for perspective or
// orthographic projections. (ScreenSpaceToViewSpaceDepth() from XeGTAO gives the same result for perspective only.)
float ScreenDepthToViewDepth(float screenDepth)
{
	const float z = u_Camera.InverseProjectionMatrix[2].z * screenDepth + u_Camera.InverseProjectionMatrix[3].z;
	const float w = u_Camera.InverseProjectionMatrix[2].w * screenDepth + u_Camera.InverseProjectionMatrix[3].w;
	return -z / w;
}

// True if the sphere is completely on the negative (outside) side of the plane
bool SphereBehindPlane(Sphere sphere, Plane plane)
{
	return dot(sphere.c, plane.N) - plane.d < -sphere.r;
}

// [FIX 3] zNear / zFar are POSITIVE view depths. The original compared the (negative) view space z directly, which with a
// right handed view space culls every light that does not touch the camera plane.
bool SphereInsideFrustum(Sphere sphere, Frustum frustum, float zNear, float zFar)
{
	const float sphereDepth = -sphere.c.z;
	if (sphereDepth + sphere.r < zNear || sphereDepth - sphere.r > zFar)
		return false;

	for (uint i = 0u; i < 4u; i++)
	{
		if (SphereBehindPlane(sphere, frustum.Planes[i]))
			return false;
	}

	return true;
}

void AABBFromMinMax(inout AABB aabb, vec3 minP, vec3 maxP)
{
	aabb.c = (minP + maxP) * 0.5f;
	aabb.e = abs(maxP - aabb.c);
}

bool SphereIntersectsAABB(in Sphere sphere, in AABB aabb)
{
	vec3 delta = max(vec3(0.0f), abs(aabb.c - sphere.c) - aabb.e);
	return dot(delta, delta) <= sphere.r * sphere.r;
}

// Flattened array index to 2D array index
uvec2 Unflatten2D(uint idx, uint dim)
{
	return uvec2(idx % dim, idx / dim);
}

// [FIX 3] Depth bounds are POSITIVE view depths so cellEnd >= cellStart always holds and the shift amount stays in [0, 31].
// (In the original with a right handed view space depthRangeRecip was negative, cellEnd < cellStart and
// "31u - (cellEnd - cellStart)" wrapped around to a shift >= 32, which is undefined behaviour.)
uint ConstructEntityMask(float depthRangeMin, float depthRangeRecip, float sphereDepth, float sphereRadius)
{
	const float fMin = sphereDepth - sphereRadius;
	const float fMax = sphereDepth + sphereRadius;
	const uint cellStart = uint(clamp(floor((fMin - depthRangeMin) * depthRangeRecip), 0.0f, 31.0f));
	const uint cellEnd   = uint(clamp(floor((fMax - depthRangeMin) * depthRangeRecip), 0.0f, 31.0f));

	uint mask = 0xFFFFFFFFu;
	mask >>= (31u - (cellEnd - cellStart));
	mask <<= cellStart;
	return mask;
}

// [FIX 4] Tight bounding sphere of what CalculateSpotLights() in IrisPBRStatic.glsl actually lights: a spherical sector with
// radius = Range and half angle = Angle / 2, where Angle is the FULL cone angle in DEGREES.
// light.Direction points from the lit surface TOWARDS the light (dot(Li, light.Direction) in the PBR shader), so the cone
// opens along -Direction (that is also why the original used "Position - Direction * r").
Sphere SpotLightBoundingSphere(vec3 positionVS, vec3 coneAxisVS, float range, float fullAngleDegrees)
{
	const float halfAngle = radians(fullAngleDegrees * 0.5f);
	const float cosHalf = cos(halfAngle);

	Sphere sphere;
	if (cosHalf >= 0.70710678f)
	{
		// Half angle <= 45 degrees: smallest sphere through the apex and the rim of the cap
		sphere.r = range / (2.0f * cosHalf);
		sphere.c = positionVS + coneAxisVS * sphere.r;
	}
	else if (cosHalf > 0.0f)
	{
		// 45 < half angle < 90 degrees: sphere centered on the rim circle
		sphere.r = range * sin(halfAngle);
		sphere.c = positionVS + coneAxisVS * (range * cosHalf);
	}
	else
	{
		// Half angle >= 90 degrees (hemisphere or wider): just use the point light sphere
		sphere.r = range;
		sphere.c = positionVS;
	}

	return sphere;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Light Culling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

shared uint uMinDepth;
shared uint uMaxDepth;
shared uint uDepthMask;

shared uint visiblePointLightCount;
shared uint visibleSpotLightCount;

// Shared local storage for visible indices, will be written out to the global buffer at the end
shared int visiblePointLightIndices[IR_MAX_POINT_LIGHT_COUNT];
shared int visibleSpotLightIndices[IR_MAX_SPOT_LIGHT_COUNT];

layout(local_size_x = IR_LIGHT_CULLING_WORKGROUP_SIZE, local_size_y = IR_LIGHT_CULLING_WORKGROUP_SIZE, local_size_z = 1) in;
void main()
{
	const uvec2 tileID = gl_WorkGroupID.xy;
	// gl_NumWorkGroups.x == m_LightCullingWorkGroups.x == u_RendererData.TilesCountX used by IrisPBRStatic.glsl
	const uint tileIndex = tileID.y * gl_NumWorkGroups.x + tileID.x;
	const ivec2 depthMapSize = textureSize(u_DepthMap, 0);
	const uint threadCount = IR_LIGHT_CULLING_WORKGROUP_SIZE * IR_LIGHT_CULLING_WORKGROUP_SIZE;

	if (gl_LocalInvocationIndex == 0u)
	{
		uMinDepth = 0xFFFFFFFFu;
		uMaxDepth = 0u;
		uDepthMask = 0u;

		visiblePointLightCount = 0u;
		visibleSpotLightCount = 0u;
	}

	// Step 1: Fetch the depth samples of this thread and reduce min / max across the work group
	const uint numSamples = IR_LIGHT_CULLING_GRANULARITY * IR_LIGHT_CULLING_GRANULARITY;
	float depth[numSamples];
	float depthMinUnrolled = 10000000.0f;
	float depthMaxUnrolled = -10000000.0f;

	for (uint s = 0u; s < numSamples; ++s)
	{
		ivec2 pixel = ivec2(gl_GlobalInvocationID.xy * IR_LIGHT_CULLING_GRANULARITY + Unflatten2D(s, IR_LIGHT_CULLING_GRANULARITY));
		pixel = min(pixel, depthMapSize - 1); // The last row / column of work groups goes past the edge of the screen
		depth[s] = texelFetch(u_DepthMap, pixel, 0).r;
		depthMinUnrolled = min(depthMinUnrolled, depth[s]);
		depthMaxUnrolled = max(depthMaxUnrolled, depth[s]);
	}

	barrier(); // Shared variable initialization above has to be visible before the atomics

	const float waveLocalMin = subgroupMin(depthMinUnrolled);
	const float waveLocalMax = subgroupMax(depthMaxUnrolled);
	if (subgroupElect())
	{
		// Depth values are positive floats so their bit patterns sort the same way as the floats
		atomicMin(uMinDepth, floatBitsToUint(waveLocalMin));
		atomicMax(uMaxDepth, floatBitsToUint(waveLocalMax));
	}

	barrier();

	// Reversed Z buffer: the biggest value is the closest one
	const float fMinDepth = uintBitsToFloat(uMaxDepth);                 // Closest
	const float fMaxDepth = max(0.000001f, uintBitsToFloat(uMinDepth)); // Farthest

	// [FIX 3] Positive view depths
	const float minDepthVS = ScreenDepthToViewDepth(fMinDepth);
	const float maxDepthVS = ScreenDepthToViewDepth(fMaxDepth);
	const float nearClipVS = ScreenDepthToViewDepth(1.0f);

	// Step 2: Tile frustum (4 side planes) and the AABB around the tile's depth bounds, both in view space.
	// Computed by every thread straight into registers, like WickedEngine does (cheap, no extra barrier needed).
	// Corner order: [0] = (min.x, min.y), [1] = (max.x, min.y), [2] = (min.x, max.y), [3] = (max.x, max.y) in pixels.
	const vec2 tileMinPx = vec2(tileID * TILED_CULLING_BLOCKSIZE);
	const vec2 tileMaxPx = vec2((tileID + 1u) * TILED_CULLING_BLOCKSIZE);
	const vec2 cornersPx[4] = vec2[4](
		vec2(tileMinPx.x, tileMinPx.y),
		vec2(tileMaxPx.x, tileMinPx.y),
		vec2(tileMinPx.x, tileMaxPx.y),
		vec2(tileMaxPx.x, tileMaxPx.y)
	);

	Frustum frustum;
	{
		// [FIX 5] The side planes go through the camera, so build them from two FIXED depths (near clip plane and 0.5)
		// instead of the tile's min / max depth: those are equal for a flat tile (e.g. a wall facing the camera), which
		// makes cross() return 0 and normalize() return NaN. Works for perspective and orthographic projections.
		vec3 n[4];
		vec3 f[4];
		for (uint i = 0u; i < 4u; i++)
		{
			n[i] = ScreenToView(vec4(cornersPx[i], 1.0f, 1.0f)).xyz;
			f[i] = ScreenToView(vec4(cornersPx[i], 0.5f, 1.0f)).xyz;
		}

		// [FIX 2] With the Y flip removed this winding gives INWARD normals in Iris' right handed view space
		frustum.Planes[0] = ComputePlane(n[2], n[0], f[0]); // min.x side
		frustum.Planes[1] = ComputePlane(n[1], n[3], f[1]); // max.x side
		frustum.Planes[2] = ComputePlane(n[0], n[1], f[0]); // min.y side
		frustum.Planes[3] = ComputePlane(n[3], n[2], f[2]); // max.y side
	}

	AABB tileAABB;
	{
		// The frustum is asymmetric so we must consider all corners
		vec3 minAABB = vec3(10000000.0f);
		vec3 maxAABB = vec3(-10000000.0f);
		for (uint i = 0u; i < 4u; i++)
		{
			const vec3 nearCorner = ScreenToView(vec4(cornersPx[i], fMinDepth, 1.0f)).xyz;
			const vec3 farCorner = ScreenToView(vec4(cornersPx[i], fMaxDepth, 1.0f)).xyz;
			minAABB = min(minAABB, min(nearCorner, farCorner));
			maxAABB = max(maxAABB, max(nearCorner, farCorner));
		}

		AABBFromMinMax(tileAABB, minAABB, maxAABB);
	}

	// Step 3: 2.5D culling, split the tile's [minDepthVS, maxDepthVS] range into 32 cells and mark the cells containing geometry
	// [FIX 5] Guard against a zero depth range (flat tile), 31 / 0 = inf and 0 * inf = NaN
	const float depthRangeRecip = 31.0f / max(maxDepthVS - minDepthVS, 0.000001f);
	uint depthMaskUnrolled = 0u;
	for (uint s = 0u; s < numSamples; ++s)
	{
		const float sampleDepthVS = ScreenDepthToViewDepth(depth[s]);
		const uint depthMaskCellIndex = uint(clamp(floor((sampleDepthVS - minDepthVS) * depthRangeRecip), 0.0f, 31.0f));
		depthMaskUnrolled |= 1u << depthMaskCellIndex;
	}

	const uint waveDepthMask = subgroupOr(depthMaskUnrolled);
	if (subgroupElect())
		atomicOr(uDepthMask, waveDepthMask);

	barrier();

	const uint depthMask = uDepthMask;

	// Step 4: Cull lights, threads are parallelized over the lights
	const uint pointLightCount = min(u_PointLights.LightCount, c_MaxPointLights);
	for (uint lightIndex = gl_LocalInvocationIndex; lightIndex < pointLightCount; lightIndex += threadCount)
	{
		Sphere sphere;
		sphere.c = (u_Camera.ViewMatrix * vec4(u_PointLights.Lights[lightIndex].Position, 1.0f)).xyz;
		sphere.r = u_PointLights.Lights[lightIndex].Radius; // Attenuation in IrisPBRStatic.glsl reaches exactly 0 at Radius

		if (SphereInsideFrustum(sphere, frustum, nearClipVS, maxDepthVS) &&
			SphereIntersectsAABB(sphere, tileAABB) &&
			(depthMask & ConstructEntityMask(minDepthVS, depthRangeRecip, -sphere.c.z, sphere.r)) != 0u)
		{
			const uint offset = atomicAdd(visiblePointLightCount, 1u);
			if (offset < c_MaxPointLights) // [FIX 5] Do not overflow the shared array
				visiblePointLightIndices[offset] = int(lightIndex);
		}
	}

	const uint spotLightCount = min(u_SpotLights.LightCount, c_MaxSpotLights);
	for (uint lightIndex = gl_LocalInvocationIndex; lightIndex < spotLightCount; lightIndex += threadCount)
	{
		const SpotLight light = u_SpotLights.Lights[lightIndex];
		const vec3 positionVS = (u_Camera.ViewMatrix * vec4(light.Position, 1.0f)).xyz;
		const vec3 coneAxisVS = -normalize(mat3(u_Camera.ViewMatrix) * light.Direction); // Cone opens along -Direction

		const Sphere sphere = SpotLightBoundingSphere(positionVS, coneAxisVS, light.Range, light.Angle);

		if (SphereInsideFrustum(sphere, frustum, nearClipVS, maxDepthVS) &&
			SphereIntersectsAABB(sphere, tileAABB) &&
			(depthMask & ConstructEntityMask(minDepthVS, depthRangeRecip, -sphere.c.z, sphere.r)) != 0u)
		{
			const uint offset = atomicAdd(visibleSpotLightCount, 1u);
			if (offset < c_MaxSpotLights) // [FIX 5] Do not overflow the shared array
				visibleSpotLightIndices[offset] = int(lightIndex);
		}
	}

	barrier();

	// Step 5: Write the tile's lists to the global buffers (all threads help), terminated with -1 unless completely full.
	// Same layout as LightCulling.glsl: tileIndex * IR_MAX_*_LIGHT_COUNT
	const uint pointLightOffset = tileIndex * c_MaxPointLights;
	const uint spotLightOffset = tileIndex * c_MaxSpotLights;
	const uint finalPointCount = min(visiblePointLightCount, c_MaxPointLights);
	const uint finalSpotCount = min(visibleSpotLightCount, c_MaxSpotLights);

	for (uint i = gl_LocalInvocationIndex; i < finalPointCount; i += threadCount)
		s_VisiblePointLightIndicesBuffer.Indices[pointLightOffset + i] = visiblePointLightIndices[i];

	for (uint i = gl_LocalInvocationIndex; i < finalSpotCount; i += threadCount)
		s_VisibleSpotLightIndicesBuffer.Indices[spotLightOffset + i] = visibleSpotLightIndices[i];

	if (gl_LocalInvocationIndex == 0u)
	{
		if (finalPointCount < c_MaxPointLights)
			s_VisiblePointLightIndicesBuffer.Indices[pointLightOffset + finalPointCount] = -1;

		if (finalSpotCount < c_MaxSpotLights)
			s_VisibleSpotLightIndicesBuffer.Indices[spotLightOffset + finalSpotCount] = -1;
	}
}