#version 450 core
#stage compute
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require

// References:
// - SIGGRAPH 2011 - Rendering in Battlefield 3
// - Implementation mostly adapted from https://github.com/bcrusco/Forward-Plus-Renderer

// TODO: Should we add AABBs? <https://wickedengine.net/2018/01/optimizing-tile-based-light-culling/>
// TODO: Look into 2.5D culling

#define TILE_SIZE 16
#define TILED_CULLING_BLOCKSIZE 16 // TODO: Move into the renderer as a define
#define TILED_CULLING_GRANULARITY 2 // TODO: Move into the renderer as a define

layout(set = 2, binding = 0) uniform sampler2D u_DepthMap;

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper Functions and structures
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Plane
{
	vec3	N;		// Plane normal.
	float	d;		// Distance to origin.
};

// Compute a plane from 3 noncollinear points that form a triangle.
// This equation assumes a right-handed (counter-clockwise winding order) 
// coordinate system to determine the direction of the plane normal.
Plane ComputePlane(vec3 p0, vec3 p1, vec3 p2)
{
	Plane plane;

	vec3 v0 = p1 - p0;
	vec3 v2 = p2 - p0;

	plane.N = normalize(cross(v0, v2));
//	plane.N = normalize(cross(v2, v0));

	// Compute the distance to the origin using p0.
	plane.d = dot(plane.N, p0);

	return plane;
}

// Four planes of a view frustum (in view space).
// The planes are:
// * Left,
// * Right,
// * Top,
// * Bottom.
// The back and/or front planes can be computed from depth values in the 
// light culling compute shader.
struct Frustum
{
	Plane Planes[4];	// left, right, top, bottom frustum planes.
};

// Convert clip space coordinates to view space
vec4 ClipToView(vec4 clip)
{
	// View space position.
	vec4 view = u_Camera.InverseProjectionMatrix * clip;
	
	// Perspective projection.
	view /= view.w;

	return view;
}

// Convert screen space coordinates to view space.
vec4 ScreenToView(vec4 screen, vec2 dim_rcp)
{
	// Convert to normalized texture coordinates
	vec2 texCoord = screen.xy * dim_rcp;

	// Convert to clip space
	vec4 clip = vec4(vec2(texCoord.x, 1.0f - texCoord.y) * 2.0f - 1.0f, screen.z, screen.w);
//	vec4 clip = vec4(texCoord * 2.0f - 1.0f, screen.z, screen.w);
//	vec4 clip = vec4(1.0f - vec2(texCoord.x, 1.0f - texCoord.y) * 2.0f, screen.z, screen.w);

	return ClipToView(clip);
}

// From XeGTAO
float ScreenSpaceToViewSpaceDepth(const float screenDepth)
{
	float depthLinearizeMul = u_Camera.DepthUnpackConsts.x;
	float depthLinearizeAdd = u_Camera.DepthUnpackConsts.y;
	// Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) * cameraClipFar"
	return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

struct Sphere
{
	vec3 c;	 // Center point.
	float r;	// Radius.
};

bool SphereInsidePlane(Sphere sphere, Plane plane)
{
	return dot(sphere.c, plane.N) - plane.d < -sphere.r;
}

bool SphereInsideFrustum(Sphere sphere, Frustum frustum, float zNear, float zFar) // this can only be used in view space
{
	bool result = true;

	result = ((sphere.c.z + sphere.r < zNear || sphere.c.z - sphere.r > zFar) ? false : result);
	result = ((SphereInsidePlane(sphere, frustum.Planes[0])) ? false : result);
	result = ((SphereInsidePlane(sphere, frustum.Planes[1])) ? false : result);
	result = ((SphereInsidePlane(sphere, frustum.Planes[2])) ? false : result);
	result = ((SphereInsidePlane(sphere, frustum.Planes[3])) ? false : result);

	return result;
}

struct AABB
{
	vec3 c; // center
	vec3 e; // half extents
};

vec3 AABB_GetMin(in AABB aabb)
{
	return aabb.c - aabb.e;
}

vec3 AABB_GetMax(in AABB aabb)
{
	return aabb.c + aabb.e;
}

void AABBfromMinMax(inout AABB aabb, vec3 _min, vec3 _max)
{
	aabb.c = (_min + _max) * 0.5f;
	aabb.e = abs(_max - aabb.c);
}

bool SphereIntersectsAABB(in Sphere sphere, in AABB aabb)
{
	vec3 vDelta = max(vec3(0.0f), abs(aabb.c - sphere.c) - aabb.e);
	float fDistSq = dot(vDelta, vDelta);
	return fDistSq <= sphere.r * sphere.r;
}

// flattened array index to 2D array index
ivec2 unflatten2D(uint idx, int dim)
{
	return ivec2(idx % dim, idx / dim);
}

uint ConstructEntityMask(in float depthRangeMin, in float depthRangeRecip, in Sphere bounds)
{
    // Construct a mask based on the view space depth bounds of the entity (Sphere)
    float fMin = bounds.c.z - bounds.r;
    float fMax = bounds.c.z + bounds.r;
    uint entitymaskcellindexSTART = uint(clamp(floor((fMin - depthRangeMin) * depthRangeRecip), 0.0f, 31.0f));
    uint entitymaskcellindexEND   = uint(clamp(floor((fMax - depthRangeMin) * depthRangeRecip), 0.0f, 31.0f));

    // Optimized mask construction:
    uint uLightMask = 0xFFFFFFFF;
    uLightMask >>= (31u - (entitymaskcellindexEND - entitymaskcellindexSTART));
    uLightMask <<= entitymaskcellindexSTART;

    return uLightMask;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Light Culling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

shared uint uMinDepth;
shared uint uMaxDepth;
shared uint uDepthMask;

shared Frustum groupFrustum;
shared AABB groupAABB;

shared uint visiblePointLightCount;
shared uint visibleSpotLightCount;

// Shared local storage for visible indices, will be written out to the global buffer at the end
shared int visiblePointLightIndices[IR_MAX_POINT_LIGHT_COUNT];
shared int visibleSpotLightIndices[IR_MAX_SPOT_LIGHT_COUNT];

layout(local_size_x = IR_LIGHT_CULLING_WORKGROUP_SIZE, local_size_y = IR_LIGHT_CULLING_WORKGROUP_SIZE, local_size_z = 1) in;
void main()
{
    ivec2 tileID = ivec2(gl_WorkGroupID.xy);
    ivec2 tileNumber = ivec2(gl_NumWorkGroups.xy);
    uint index = tileID.y * tileNumber.x + tileID.x;

	ivec2 textureDimension = textureSize(u_DepthMap, 0);
	vec2 invTextureDimension = vec2(1.0f / textureDimension);

	if (gl_LocalInvocationIndex == 0)
	{
		uMinDepth = 0xffffffff;
		uMaxDepth = 0;
		uDepthMask = 0;

		visiblePointLightCount = 0;
		visibleSpotLightCount = 0;
	}

	const uint numSamples = TILED_CULLING_GRANULARITY * TILED_CULLING_GRANULARITY;
	float depth[numSamples];
    float depthMinUnrolled = 10000000.0f;
    float depthMaxUnrolled = -10000000.0f;

	#pragma unroll
	for (uint granularity = 0u; granularity < numSamples; ++granularity)
    {
        // Compute pixel coordinate:
        ivec2 pixel = ivec2(gl_GlobalInvocationID.xy * ivec2(TILED_CULLING_GRANULARITY) + unflatten2D(granularity, TILED_CULLING_GRANULARITY));
        pixel = min(pixel, textureDimension - 1);  // clamp to valid range
        depth[granularity] = texelFetch(u_DepthMap, pixel, 0).r;
        depthMinUnrolled = min(depthMinUnrolled, depth[granularity]);
        depthMaxUnrolled = max(depthMaxUnrolled, depth[granularity]);
    }

	barrier();

	float waveLocalMin = subgroupMin(depthMinUnrolled);
	float waveLocalMax = subgroupMax(depthMaxUnrolled);

	if (gl_SubgroupInvocationID == 0)
	{
		 // atomicMin/atomicMax for uints, convert float to uint bits.
		atomicMin(uMinDepth, floatBitsToUint(waveLocalMin));
		atomicMax(uMaxDepth, floatBitsToUint(waveLocalMax));
	}

	barrier();

	// Reversed Z buffer
	float fMinDepth = uintBitsToFloat(uMaxDepth);
	float fMaxDepth = uintBitsToFloat(uMinDepth);
	fMaxDepth = max(0.000001f, fMaxDepth);

	// Frustum and AABB calculation
	{
		// Compute view-space frustum corners
		vec3 viewSpace[8];

		// Top left point, near
		viewSpace[0] = ScreenToView(vec4(gl_WorkGroupID.xy * TILED_CULLING_BLOCKSIZE, fMinDepth, 1.0f), invTextureDimension).xyz;
		// Top right point, near
		viewSpace[1] = ScreenToView(vec4(vec2(gl_WorkGroupID.x + 1, gl_WorkGroupID.y) * TILED_CULLING_BLOCKSIZE, fMinDepth, 1.0f), invTextureDimension).xyz;
		// Bottom left point, near
		viewSpace[2] = ScreenToView(vec4(vec2(gl_WorkGroupID.x, gl_WorkGroupID.y + 1) * TILED_CULLING_BLOCKSIZE, fMinDepth, 1.0f), invTextureDimension).xyz;
		// Bottom right point, near
		viewSpace[3] = ScreenToView(vec4(vec2(gl_WorkGroupID.x + 1, gl_WorkGroupID.y + 1) * TILED_CULLING_BLOCKSIZE, fMinDepth, 1.0f), invTextureDimension).xyz;
		// Top left point, far
		viewSpace[4] = ScreenToView(vec4(gl_WorkGroupID.xy * TILED_CULLING_BLOCKSIZE, fMaxDepth, 1.0f), invTextureDimension).xyz;
		// Top right point, far
		viewSpace[5] = ScreenToView(vec4(vec2(gl_WorkGroupID.x + 1, gl_WorkGroupID.y) * TILED_CULLING_BLOCKSIZE, fMaxDepth, 1.0f), invTextureDimension).xyz;
		// Bottom left point, far
		viewSpace[6] = ScreenToView(vec4(vec2(gl_WorkGroupID.x, gl_WorkGroupID.y + 1) * TILED_CULLING_BLOCKSIZE, fMaxDepth, 1.0f), invTextureDimension).xyz;
		// Bottom right point, far
		viewSpace[7] = ScreenToView(vec4(vec2(gl_WorkGroupID.x + 1, gl_WorkGroupID.y + 1) * TILED_CULLING_BLOCKSIZE, fMaxDepth, 1.0f), invTextureDimension).xyz;

		// Left plane
		groupFrustum.Planes[0] = ComputePlane(viewSpace[2], viewSpace[0], viewSpace[4]);
		// Right plane
		groupFrustum.Planes[1] = ComputePlane(viewSpace[1], viewSpace[3], viewSpace[5]);
		// Top plane
		groupFrustum.Planes[2] = ComputePlane(viewSpace[0], viewSpace[1], viewSpace[4]);
		// Bottom plane
		groupFrustum.Planes[3] = ComputePlane(viewSpace[3], viewSpace[2], viewSpace[6]);

		// We construct an AABB around the minmax depth bounds to perform tighter culling:
		// The frustum is asymmetric so we must consider all corners
		vec3 minAABB = vec3(10000000);
		vec3 maxAABB = vec3(-10000000);

		#pragma unroll
		for (uint i = 0; i < 8; i++)
		{
			minAABB = min(minAABB, viewSpace[i]);
			maxAABB = max(maxAABB, viewSpace[i]);
		}

		AABBfromMinMax(groupAABB, minAABB, maxAABB);
	}

	// Convert depth values to view space.
	float minDepthVS = ScreenToView(vec4(0.0f, 0.0f, fMinDepth, 1.0f), invTextureDimension).z;
	float maxDepthVS = ScreenToView(vec4(0.0f, 0.0f, fMaxDepth, 1.0f), invTextureDimension).z;
	float nearClipVS = ScreenToView(vec4(0.0f, 0.0f, 1.0f, 1.0f), invTextureDimension).z;

	// 2.5D Slicing
	const float depthRangeRecip = 31.0f / (maxDepthVS - minDepthVS);
	uint depthMaskUnrolled = 0;

	#pragma unroll
	for (uint granularity = 0; granularity < numSamples; ++granularity)
	{
		float realDepthVS = ScreenToView(vec4(0.0f, 0.0f, depth[granularity], 1.0f), invTextureDimension).z;
		const uint depthMaskCellIndex = uint(max(0.0f, min(31.0f, floor((realDepthVS - minDepthVS) * depthRangeRecip))));
		depthMaskUnrolled |= 1u << depthMaskCellIndex;
	}

	uint waveDepthMask = subgroupOr(depthMaskUnrolled);
    if (gl_SubgroupInvocationID == 0u)
	{
        atomicOr(uDepthMask, waveDepthMask);
    }

	barrier();

	// take out from groupshared into register
	const uint depthMask = uDepthMask;

	// Point Lights
	// Step 3: Cull lights.
    // Parallelize the threads against the lights now.
    // Can handle 256 simultaniously. Anymore lights than that and additional passes are performed
    const uint threadCount = IR_LIGHT_CULLING_WORKGROUP_SIZE * IR_LIGHT_CULLING_WORKGROUP_SIZE;
    uint passCount = (u_PointLights.LightCount + threadCount - 1) / threadCount;
    for (uint i = 0; i < passCount; i++)
    {
		// Get the lightIndex to test for this thread / pass. If the index is >= light count, then this thread can stop testing lights
		uint lightIndex = i * threadCount + gl_LocalInvocationIndex;
		if (lightIndex >= u_PointLights.LightCount)
		    break;

		vec3 positionVS = (u_Camera.ViewMatrix * vec4(u_PointLights.Lights[lightIndex].Position, 1.0f)).xyz;
		Sphere sphere;
		sphere.c = positionVS.xyz;
		sphere.r = u_PointLights.Lights[lightIndex].Radius;

		if (SphereInsideFrustum(sphere, groupFrustum, nearClipVS, maxDepthVS))
		{
			if (SphereIntersectsAABB(sphere, groupAABB))
			{
				if ((depthMask & ConstructEntityMask(minDepthVS, depthRangeRecip, sphere)) != 0)
				{
					// Add index to the shared array of visible indices
					uint offset = atomicAdd(visiblePointLightCount, 1);
					visiblePointLightIndices[offset] = int(lightIndex);
				}
			}
		}
    }

	passCount = (u_SpotLights.LightCount + threadCount - 1) / threadCount;
	for (uint i = 0; i < passCount; i++)
	{
		// Get the lightIndex to test for this thread / pass. If the index is >= light count, then this thread can stop testing lights
		uint lightIndex = i * threadCount + gl_LocalInvocationIndex;
		if (lightIndex >= u_SpotLights.LightCount)
			break;

		SpotLight light = u_SpotLights.Lights[lightIndex];
		vec3 positionVS = (u_Camera.ViewMatrix * vec4(light.Position, 1.0f)).xyz;
		vec3 directionVS = mat3(u_Camera.ViewMatrix) * light.Direction;

		// Construct a tight fitting sphere around the spotlight cone
		float coneAngleCos = cos(light.Angle);
		const float r = light.Range * 0.5f / (coneAngleCos * coneAngleCos);
		Sphere sphere;
		sphere.c = positionVS - directionVS * r;
		sphere.r = r;

		if (SphereInsideFrustum(sphere, groupFrustum, nearClipVS, maxDepthVS))
		{
			if (SphereIntersectsAABB(sphere, groupAABB))
			{
				if ((depthMask & ConstructEntityMask(minDepthVS, depthRangeRecip, sphere)) != 0)
				{
					// Add index to the shared array of visible indices
					uint offset = atomicAdd(visibleSpotLightCount, 1);
					visibleSpotLightIndices[offset] = int(lightIndex);
				}
			}
		}
	}

	barrier();

    // One thread should fill the global light buffer
    if (gl_LocalInvocationIndex == 0)
    {
		const uint pointLightOffset = index * IR_MAX_POINT_LIGHT_COUNT; // Determine position in global buffer
		const uint spotLightOffset = index * IR_MAX_SPOT_LIGHT_COUNT; // Determine position in global buffer

		for (uint i = 0; i < visiblePointLightCount; i++) 
		{
			s_VisiblePointLightIndicesBuffer.Indices[pointLightOffset + i] = visiblePointLightIndices[i];
		}

		for (uint i = 0; i < visibleSpotLightCount; i++)
		{
			s_VisibleSpotLightIndicesBuffer.Indices[spotLightOffset + i] = visibleSpotLightIndices[i];
		}

		if (visiblePointLightCount != IR_MAX_POINT_LIGHT_COUNT)
		{
		    // Unless we have totally filled the entire array, mark it's end with -1
		    // Final shader step will use this to determine where to stop (without having to pass the light count)
			s_VisiblePointLightIndicesBuffer.Indices[pointLightOffset + visiblePointLightCount] = -1;
		}

		if (visibleSpotLightCount != IR_MAX_SPOT_LIGHT_COUNT)
		{
			// Unless we have totally filled the entire array, mark it's end with -1
			// Final shader step will use this to determine where to stop (without having to pass the light count)
			s_VisibleSpotLightIndicesBuffer.Indices[spotLightOffset + visibleSpotLightCount] = -1;
		}
    }
}