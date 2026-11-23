#version 450 core
#stage compute
    
// Adapted from: https://wickedengine.net/

// Output buffer
layout(std430, set = 1, binding = 9) buffer LuminanceHistogramBuffer
{
	uint Bins[];
} s_LuminanceHistogram;

// Input image
layout(set = 2, binding = 0) uniform sampler2D u_Texture;

layout(push_constant) uniform Uniforms
{
    ivec2 Resolution;
    vec2 Resolution_Rcp;
    vec4 Params0; // (.x = LuminanceAdaptionate, .y = LuminanceLogMin, .z = LuminanceLogMax, .w = LuminanceLogRange)
    vec4 Params1; // (.x = LuminanceLogRangeRcp, .y = LuminancePixelCount, .z = LuminanceEyeAdaptionKey, .w = FrameDeltaTime)
} u_Uniforms;

// groupshared memory is used to perform the high atomic contentions
// the atomic contention into global memory will be lower in the general case
// So even though this increases number of atomic ops, performance will be better
shared uint groupSharedBins[IR_LUMINANCE_NUM_HISTOGRAM_BINS];

layout(local_size_x = IR_LUMINANCE_WORKGROUP_SIZE, local_size_y = IR_LUMINANCE_WORKGROUP_SIZE, local_size_z = 1) in;
void main()
{
    const uvec2 location = uvec2(gl_GlobalInvocationID.xy);

    // zero out our shared staging
    groupSharedBins[gl_LocalInvocationIndex] = 0u;

    barrier();

    if (location.x < uint(u_Uniforms.Resolution.x) && location.y < uint(u_Uniforms.Resolution.y))
    {
        const vec2 uv = (vec2(location.xy) + 0.5f) * u_Uniforms.Resolution_Rcp;
        const vec3 color = textureLod(u_Texture, uv, 0).rgb;
        const float luminance = dot(color, vec3(0.2126f, 0.7152f, 0.0722f));

        uint binIndex = 0u;
        if (luminance > 0.001f)
		{
			const float logLuminance = clamp((log2(luminance) - u_Uniforms.Params0.y) * u_Uniforms.Params1.x, 0.0f, 1.0f);
			binIndex = uint(logLuminance * float(IR_LUMINANCE_NUM_HISTOGRAM_BINS - 2) + 1.0f);
		}

        atomicAdd(groupSharedBins[binIndex], 1u);
    }

    barrier();

    atomicAdd(s_LuminanceHistogram.Bins[(IR_LUMINANCE_BUFFER_OFFSET_HISTOGRAM + gl_LocalInvocationIndex * 4) / 4], groupSharedBins[gl_LocalInvocationIndex]);
}
