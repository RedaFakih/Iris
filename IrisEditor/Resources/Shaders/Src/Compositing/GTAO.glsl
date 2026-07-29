#version 450 core
#stage compute

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2021, Intel Corporation 
// 
// SPDX-License-Identifier: MIT
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// XeGTAO is based on GTAO/GTSO "Jimenez et al. / Practical Real-Time Strategies for Accurate Indirect Occlusion", 
// https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
// 
// Implementation:  Filip Strugar (filip.strugar@intel.com), Steve Mccalla <stephen.mccalla@intel.com>         (\_/)
// Details:         https://github.com/GameTechDev/XeGTAO                                                     (")_(")
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

layout(set = 2, binding = 0, r32ui) uniform writeonly uimage2D o_AOwBentNormals; // output AO term (includes bent normals if enabled - packed as R11G11B10 scaled by AO)
layout(set = 2, binding = 1, r8) uniform writeonly image2D o_Edges; // output depth-based edges used by the denoiser

layout(set = 0, binding = 0) uniform usampler2D u_HilbertLUT;

layout(set = 2, binding = 2) uniform sampler2D u_HZB; 
layout(set = 2, binding = 3) uniform sampler2D u_ViewNormal; 

layout(push_constant) uniform GTAOConstants
{
    vec2 NDCToViewMul_x_PixelSize;
    float EffectRadius; // world (viewspace) maximum size of the shadow
    float EffectFalloffRange;

    float RadiusMultiplier;
    float FinalValuePower;
    float DenoiseBlurBeta;
    uint HalfRes; // Boolean

    float SampleDistributionPower;
    float ThinOccluderCompensation;
    float DepthMIPSamplingOffset;
    int NoiseIndex; // frameIndex % 64 if using TAA or 0 otherwise

    vec2 HZBUVFactor;
    float ShadowTolerance;
    float Padding0;
} u_GTAOConstants;

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

#define XE_GTAO_DEPTH_MIP_LEVELS      5                   // this one is hard-coded to 5 for now
#define XE_GTAO_PI               	 (3.1415926535897932384626433832795f)
#define XE_GTAO_PI_HALF              (1.5707963267948966192313216916398f)
#define XE_GTAO_OCCLUSION_TERM_SCALE (1.5f) // for packing in UNORM (because raw, pre-denoised occlusion term can overshoot 1 but will later average out to 1)

uint XeGTAO_FLOAT4_to_R8G8B8A8_UNORM(vec4 unpackedInput)
{
    return (
        (uint(clamp(unpackedInput.x, 0.0f, 1.0f) * 255.0f + 0.5f))       |
        (uint(clamp(unpackedInput.y, 0.0f, 1.0f) * 255.0f + 0.5f) << 8)  |
        (uint(clamp(unpackedInput.z, 0.0f, 1.0f) * 255.0f + 0.5f) << 16) |
        (uint(clamp(unpackedInput.w, 0.0f, 1.0f) * 255.0f + 0.5f) << 24)
    );
}

// Inputs are screen XY and viewspace depth, output is viewspace position
vec3 XeGTAO_ComputeViewSpacePosition(const vec2 screenPos, const float viewspaceDepth)
{
    // vec3 ret = vec3(0.0f);
    // ret.xy = fma(u_Camera.NDCToViewMul, screenPos, u_Camera.NDCToViewAdd) * viewspaceDepth;
    // ret.z = viewspaceDepth;


    // return vec3(vec2(fma(u_Camera.NDCToViewMul, screenPos, u_Camera.NDCToViewAdd) * viewspaceDepth), viewspaceDepth);

    vec3 ret;
    // Replaced fma() with standard math to avoid strict type-matching errors
    ret.xy = (u_Camera.NDCToViewMul * screenPos + u_Camera.NDCToViewAdd) * viewspaceDepth;
    ret.z = viewspaceDepth;
    return vec3(ret.x , ret.y, ret.z);
}

vec4 XeGTAO_CalculateEdges(const float centerZ, const float leftZ,  const float rightZ, const float topZ, const float bottomZ)
{
    vec4 edgesLRTB = vec4(leftZ, rightZ, topZ, bottomZ) - float(centerZ);

    float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5f;
    float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5f;

    vec4 edgesLRTBSlopeAdjusted = edgesLRTB + vec4(slopeLR, -slopeLR, slopeTB, -slopeTB);
    edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));

    return vec4(clamp((1.25f - edgesLRTB / (centerZ * 0.011f)), 0.0f, 1.0f));
}

// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
float XeGTAO_PackEdges(vec4 edgesLRTB)
{
    // integer version:
    // edgesLRTB = clamp(edgesLRTB, 0.0f, 1.0f) * 2.9.xxxx + 0.5.xxxx;
    // return ((uint(edgesLRTB.x)) << 6) + ((uint(edgesLRTB.y)) << 4) + ((uint(edgesLRTB.z)) << 2) + ((uint(edgesLRTB.w)));
    // 
    // optimized, should be same as above
    edgesLRTB = round(clamp(edgesLRTB, 0.0f, 1.0f) * 2.9f);
    return dot(edgesLRTB, vec4(64.0f / 255.0f, 16.0f / 255.0f, 4.0f / 255.0f, 1.0f / 255.0f));
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html, [Drobot2014a] Low Level Optimizations for GCN, https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf slide 63
float XeGTAO_FastSqrt(float x)
{
    return float(intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(x) >> 1)));
}

// input [-1, 1] and output [0, PI], from https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
float XeGTAO_FastACos(float inX)
{
    float x = abs(inX);
    float res = -0.156583 * x + XE_GTAO_PI_HALF;
    res *= XeGTAO_FastSqrt(1.0 - x);
    return (inX >= 0) ? res : XE_GTAO_PI - res;
}

uint XeGTAO_EncodeVisibilityBentNormal(float visibility, vec3 bentNormal)
{
    return XeGTAO_FLOAT4_to_R8G8B8A8_UNORM(vec4(bentNormal * 0.5f + 0.5f, visibility));
}

void XeGTAO_OutputWorkingTerm(const uvec2 pixCoord, float visibility, vec3 bentNormal)
{
    visibility = float(clamp(visibility / XE_GTAO_OCCLUSION_TERM_SCALE, 0.0f, 1.0f));
    imageStore(o_AOwBentNormals, ivec2(pixCoord), uvec4(XeGTAO_EncodeVisibilityBentNormal(visibility, bentNormal), 0.0f, 0.0f, 0.0f));
}

// "Efficiently building a matrix to rotate one vector to another"
// http://cs.brown.edu/research/pubs/pdfs/1999/Moller-1999-EBA.pdf / https://dl.acm.org/doi/10.1080/10867651.1999.10487509
// (using https://github.com/assimp/assimp/blob/master/include/assimp/matrix3x3.inl#L275 as a code reference as it seems to be best)
mat3 XeGTAO_RotFromToMatrix(vec3 from, vec3 to)
{
    const float e = dot(from, to);
    const float f = abs(e);

    // WARNING: This has not been tested/worked through, especially not for 16bit floats; seems to work in our special use case (from is always {0, 0, -1}) but wouldn't use it in general
    if (f > float(1.0f - 0.0003f))
        return mat3(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

    const vec3 v = cross(from, to);
    /* ... use this hand optimized version (9 mults less) */
    const float h = (1.0) / (1.0 + e);      /* optimization by Gottfried Chen */
    const float hvx = h * v.x;
    const float hvz = h * v.z;
    const float hvxy = hvx * v.y;
    const float hvxz = hvx * v.z;
    const float hvyz = hvz * v.y;

    mat3 mtx;
    mtx[0][0] = e + hvx * v.x;
    mtx[0][1] = hvxy - v.z;
    mtx[0][2] = hvxz + v.y;

    mtx[1][0] = hvxy + v.z;
    mtx[1][1] = e + h * v.y * v.y;
    mtx[1][2] = hvyz - v.x;

    mtx[2][0] = hvxz - v.y;
    mtx[2][1] = hvyz + v.x;
    mtx[2][2] = e + hvz * v.z;

    return mtx;
}

float LinearizeDepth(const float screenDepth)
{
    float depthLinearizeMul = u_Camera.DepthUnpackConsts.x;
    float depthLinearizeAdd = u_Camera.DepthUnpackConsts.y;
    // Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) * cameraClipFar"
    return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

vec4 LinearizeDepth(const vec4 screenDepths)
{
    return vec4(LinearizeDepth(screenDepths.x), LinearizeDepth(screenDepths.y), LinearizeDepth(screenDepths.z), LinearizeDepth(screenDepths.w));
}

void XeGTAO_MainPass(const ivec2 outputPixCoord, const ivec2 inputPixCoords, float sliceCount, float stepsPerSlice, const vec2 localNoise)
{
    vec4 viewspaceNormalLuminance = texelFetch(u_ViewNormal, inputPixCoords, 0);
    vec3 viewspaceNormal = viewspaceNormalLuminance.xyz;
    viewspaceNormal.yz = -viewspaceNormalLuminance.yz;

    vec2 normalizedScreenPos = (vec2(inputPixCoords) + vec2(0.5f)) * u_ScreenData.InverseFullResolution;

    vec2 gatherUV = vec2(inputPixCoords) * u_ScreenData.InverseFullResolution * u_GTAOConstants.HZBUVFactor;
    vec4 deviceZs = textureGather(u_HZB, gatherUV, 0);
    vec4 valuesUL = LinearizeDepth(deviceZs);
    vec4 valuesBR = LinearizeDepth(textureGatherOffset(u_HZB, gatherUV, ivec2(1, 1), 0));

    // viewspace Z at the center
    float viewspaceZ = valuesUL.y; //u_HiZDepth.SampleLevel( u_samplerPointClamp, normalizedScreenPos, 0 ).x; 

    // viewspace Zs left top right bottom
    const float pixLZ = valuesUL.x;
    const float pixTZ = valuesUL.z;
    const float pixRZ = valuesBR.z;
    const float pixBZ = valuesBR.x;

    vec4 edgesLRTB = XeGTAO_CalculateEdges(viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ);
    imageStore(o_Edges, outputPixCoord, vec4(XeGTAO_PackEdges(edgesLRTB)));

    // Move center pixel slightly towards camera to avoid imprecision artifacts due to depth buffer imprecision; offset depends on depth texture format used
#ifdef XE_GTAO_FP32_DEPTHS
    viewspaceZ *= 0.99999f;     // this is good for FP32 depth buffer
#else
    viewspaceZ *= 0.99920f;     // this is good for FP16 depth buffer
#endif

    const vec3 pixCenterPos = XeGTAO_ComputeViewSpacePosition(normalizedScreenPos, viewspaceZ);
    const vec3 viewVec = normalize(-pixCenterPos);

    // prevents normals that are facing away from the view vector - xeGTAO struggles with extreme cases, but in Vanilla it seems rare so it's disabled by default
    // viewspaceNormal = normalize( viewspaceNormal + max( 0, -dot( viewspaceNormal, viewVec ) ) * viewVec );

    const float effectRadius = u_GTAOConstants.EffectRadius * u_GTAOConstants.RadiusMultiplier;
    const float sampleDistributionPower = u_GTAOConstants.SampleDistributionPower;
    const float thinOccluderCompensation = u_GTAOConstants.ThinOccluderCompensation;
    const float falloffRange = u_GTAOConstants.EffectFalloffRange * effectRadius;

    const float falloffFrom = effectRadius * (1.0f - u_GTAOConstants.EffectFalloffRange);

    // fadeout precompute optimisation
    const float falloffMul = -1.0f / falloffRange;
    const float falloffAdd = falloffFrom / falloffRange + 1.0f;

    float visibility = 0.0f;
    vec3 bentNormal = vec3(0.0f);

    // see "Algorithm 1" in https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
    {
        const float noiseSlice = localNoise.x;
        const float noiseSample = localNoise.y;

        // quality settings / tweaks / hacks
        const float pixelTooCloseThreshold = 1.3f;      // if the offset is under approx pixel size (pixelTooCloseThreshold), push it out to the minimum distance

        // approx viewspace pixel size at inputPixCoords; approximation of NDCToViewspace( normalizedScreenPos.xy + u_ScreenData.InvFullResolution.xy, pixCenterPos.z ).xy - pixCenterPos.xy;
        const vec2 pixelDirRBViewspaceSizeAtCenterZ = vec2(viewspaceZ) * u_GTAOConstants.NDCToViewMul_x_PixelSize;

        float screenspaceRadius = effectRadius / pixelDirRBViewspaceSizeAtCenterZ.x;

        // fade out for small screen radii 
        visibility += clamp((10.0f - screenspaceRadius) / 100.0f, 0.0f, 1.0f) * 0.5f;

        // sensible early-out for even more performance;
        if (deviceZs.y == 0.0f || screenspaceRadius < pixelTooCloseThreshold)
        {
            XeGTAO_OutputWorkingTerm(uvec2(outputPixCoord), 1.0f, viewspaceNormal);
            return;
        }

        // this is the min distance to start sampling from to avoid sampling from the center pixel (no useful data obtained from sampling center pixel)
        const float minS = pixelTooCloseThreshold / screenspaceRadius;

        for (float slice = 0; slice < sliceCount; slice++)
        {
            float sliceK = (slice + noiseSlice) / sliceCount;
            // lines 5, 6 from the paper
            float phi = sliceK * XE_GTAO_PI;
            float cosPhi = cos(phi);
            float sinPhi = sin(phi);
            vec2 omega = vec2(cosPhi, -sinPhi); // vec2 on omega causes issues with big radii

            // convert to screen units (pixels) for later use
            omega *= screenspaceRadius;

            // line 8 from the paper
            const vec3 directionVec = vec3(cosPhi, sinPhi, 0.0f);

            // line 9 from the paper
            const vec3 orthoDirectionVec = directionVec - (dot(directionVec, viewVec) * viewVec);

            // line 10 from the paper
            //axisVec is orthogonal to directionVec and viewVec, used to define projectedNormal
            const vec3 axisVec = normalize(cross(orthoDirectionVec, viewVec));

            // alternative line 9 from the paper
            // float3 orthoDirectionVec = cross( viewVec, axisVec );

            // line 11 from the paper
            vec3 projectedNormalVec = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);

            // line 13 from the paper
            float signNorm = sign(dot(orthoDirectionVec, projectedNormalVec));

            // line 14 from the paper
            float projectedNormalVecLength = length(projectedNormalVec);
            float cosNorm = clamp(dot(projectedNormalVec, viewVec) / projectedNormalVecLength, 0.0f, 1.0f);

            // line 15 from the paper
            float n = signNorm * XeGTAO_FastACos(cosNorm);

            // this is a lower weight target; not using -1 as in the original paper because it is under horizon, so a 'weight' has different meaning based on the normal
            const float lowHorizonCos0 = cos(n + XE_GTAO_PI_HALF);
            const float lowHorizonCos1 = cos(n - XE_GTAO_PI_HALF);

            // lines 17, 18 from the paper, manually unrolled the 'side' loop
            float horizonCos0 = lowHorizonCos0; //-1;   
            float horizonCos1 = lowHorizonCos1; //-1;

            for (float step = 0.0f; step < stepsPerSlice; step++)
            {
                // R1 sequence (http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/)
                const float stepBaseNoise = (slice + step * stepsPerSlice) * 0.6180339887498948482f; // <- this should unroll
                float stepNoise = fract(noiseSample + stepBaseNoise);

                // approx line 20 from the paper, with added noise
                float s = (step + stepNoise) / stepsPerSlice; // + (lpfloat2)1e-6f);

                // additional distribution modifier
                s = pow(s, sampleDistributionPower);

                // avoid sampling center pixel
                s += minS;

                // approx lines 21-22 from the paper, unrolled
                vec2 sampleOffset = s * omega;

                float sampleOffsetLength = length(sampleOffset);

                // note: when sampling, using point_point_point or point_point_linear sampler works, but linear_linear_linear will cause unwanted interpolation between neighbouring depth values on the same MIP level!
                const float mipLevel = clamp(log2(sampleOffsetLength) - u_GTAOConstants.DepthMIPSamplingOffset, 0.0f, float(XE_GTAO_DEPTH_MIP_LEVELS));

                // Snap to pixel center (more correct direction math, avoids artifacts due to sampling pos not matching depth texel center - messes up slope - but adds other 
                // artifacts due to them being pushed off the slice). Also use full precision for high res cases.
                sampleOffset = round(sampleOffset) * u_ScreenData.InverseFullResolution;

                vec2 sampleScreenPos0 = normalizedScreenPos + sampleOffset;
                float SZ0 = LinearizeDepth(textureLod(u_HZB, sampleScreenPos0 * u_GTAOConstants.HZBUVFactor, mipLevel).x);
                vec3 samplePos0 = XeGTAO_ComputeViewSpacePosition(sampleScreenPos0, SZ0);

                vec2 sampleScreenPos1 = normalizedScreenPos - sampleOffset;
                float  SZ1 = LinearizeDepth(textureLod(u_HZB, sampleScreenPos1 * u_GTAOConstants.HZBUVFactor, mipLevel).x);
                vec3 samplePos1 = XeGTAO_ComputeViewSpacePosition(sampleScreenPos1, SZ1);

                vec3 sampleDelta0 = (samplePos0 - pixCenterPos); // using lpfloat for sampleDelta causes precision issues
                vec3 sampleDelta1 = (samplePos1 - pixCenterPos); // using lpfloat for sampleDelta causes precision issues
                float sampleDist0 = length(sampleDelta0);
                float sampleDist1 = length(sampleDelta1);

                // approx lines 23, 24 from the paper, unrolled
                vec3 sampleHorizonVec0 = sampleDelta0 / sampleDist0;
                vec3 sampleHorizonVec1 = sampleDelta1 / sampleDist1;

                // this is our own thickness heuristic that relies on sooner discarding samples behind the center
                float falloffBase0 = length(vec3(sampleDelta0.x, sampleDelta0.y, sampleDelta0.z * (1.0f + thinOccluderCompensation)));
                float falloffBase1 = length(vec3(sampleDelta1.x, sampleDelta1.y, sampleDelta1.z * (1.0f + thinOccluderCompensation)));
                float weight0 = clamp(falloffBase0 * falloffMul + falloffAdd, 0.0f, 1.0f);
                float weight1 = clamp(falloffBase1 * falloffMul + falloffAdd, 0.0f, 1.0f);

                // sample horizon cos
                float shc0 = dot(sampleHorizonVec0, viewVec);
                float shc1 = dot(sampleHorizonVec1, viewVec);

                // discard unwanted samples
                shc0 = mix(lowHorizonCos0, shc0, weight0); // this would be more correct but too expensive: cos(lerp( acos(lowHorizonCos0), acos(shc0), weight0 ));
                shc1 = mix(lowHorizonCos1, shc1, weight1); // this would be more correct but too expensive: cos(lerp( acos(lowHorizonCos1), acos(shc1), weight1 ));

                // thickness heuristic - see "4.3 Implementation details, Height-field assumption considerations"
#if 0   // (disabled, not used) this should match the paper
                float newhorizonCos0 = max(horizonCos0, shc0);
                float newhorizonCos1 = max(horizonCos1, shc1);
                horizonCos0 = (horizonCos0 > shc0) ? (mix(newhorizonCos0, shc0, thinOccluderCompensation)) : (newhorizonCos0);
                horizonCos1 = (horizonCos1 > shc1) ? (mix(newhorizonCos1, shc1, thinOccluderCompensation)) : (newhorizonCos1);
#elif 0 // (disabled, not used) this is slightly different from the paper but cheaper and provides very similar results
                horizonCos0 = mix(max(horizonCos0, shc0), shc0, thinOccluderCompensation);
                horizonCos1 = mix(max(horizonCos1, shc1), shc1, thinOccluderCompensation);
#else   // this is a version where thicknessHeuristic is completely disabled
                horizonCos0 = max(horizonCos0, shc0);
                horizonCos1 = max(horizonCos1, shc1);
#endif
            }

            projectedNormalVecLength = mix(projectedNormalVecLength, 1.0f, 0.05f);

            // line ~27, unrolled
            float h0 = -XeGTAO_FastACos(horizonCos1);
            float h1 = XeGTAO_FastACos(horizonCos0);
            float iarc0 = (cosNorm + 2.0f * h0 * sin(n) - cos(2.0f * h0 - n)) / 4.0f;
            float iarc1 = (cosNorm + 2.0f * h1 * sin(n) - cos(2.0f * h1 - n)) / 4.0f;
            float localVisibility = projectedNormalVecLength * (iarc0 + iarc1);
            visibility += localVisibility;

            // see "Algorithm 2 Extension that computes bent normals b."
            float t0 = (6.0f * sin(h0 - n) - sin(3.0f * h0 - n) + 6.0f * sin(h1 - n) - sin(3.0f * h1 - n) + 16.0f * sin(n) - 3.0f * (sin(h0 + n) + sin(h1 + n))) / 12.0f;
            float t1 = (-cos(3.0f * h0 - n) - cos(3.0f * h1 - n) + 8.0f * cos(n) - 3.0f * (cos(h0 + n) + cos(h1 + n))) / 12.0f;
            vec3 localBentNormal = vec3(directionVec.x * t0, directionVec.y * t0, -t1);
            localBentNormal = (XeGTAO_RotFromToMatrix(vec3(0.0f, 0.0f, -1.0f), viewVec), localBentNormal) * projectedNormalVecLength;
            bentNormal += localBentNormal;
        }
        visibility /= sliceCount;
        visibility = pow(visibility, u_GTAOConstants.FinalValuePower * mix(1.0f, u_GTAOConstants.ShadowTolerance, viewspaceNormalLuminance.a));
        visibility = max(0.03f, visibility); // disallow total occlusion (which wouldn't make any sense anyhow since pixel is visible but also helps with packing bent normals)

        bentNormal = normalize(bentNormal);
    }

    XeGTAO_OutputWorkingTerm(uvec2(outputPixCoord), visibility, bentNormal);
}

// Engine-specific screen & temporal noise loader
vec2 SpatioTemporalNoise(uvec2 pixCoord, uint temporalIndex)    // without TAA, temporalIndex is always 0
{
    // Hilbert curve driving R2 (see https://www.shadertoy.com/view/3tB3z3)
    uint index = texelFetch(u_HilbertLUT, ivec2(pixCoord % 64u), 0).x;
    // why 288? tried out a few and that's the best so far (with XE_HILBERT_LEVEL 6U) - but there's probably better :)
    index += 288u * (temporalIndex % 64u);
    // R2 sequence - see http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
    return vec2(fract(0.5f + float(index) * vec2(0.75487766624669276005f, 0.5698402909980532659114f)));
}

// Engine-specific entry point for the second pass
layout(local_size_x = IR_GTAO_COMPUTE_WORKGROUP_SIZE, local_size_y = IR_GTAO_COMPUTE_WORKGROUP_SIZE, local_size_z = 1) in;
void main()
{
    const ivec2 outputPixCoords = ivec2(gl_GlobalInvocationID.xy);
    const ivec2 inputPixCoords = outputPixCoords * (1 + int(u_GTAOConstants.HalfRes));
    XeGTAO_MainPass(outputPixCoords, inputPixCoords, 9.0f, 3.0f, SpatioTemporalNoise(uvec2(inputPixCoords), uint(u_GTAOConstants.NoiseIndex)));
}