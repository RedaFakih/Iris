#version 450 core
#stage compute
    
// Adapted from: https://wickedengine.net/

// Output buffer (read/write for this shader)
layout(std430, set = 1, binding = 9) buffer LuminanceHistogramBuffer
{
	uint Bins[];
} s_LuminanceHistogram;

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
shared uint histogram[IR_LUMINANCE_NUM_HISTOGRAM_BINS];

layout(local_size_x = IR_LUMINANCE_WORKGROUP_SIZE, local_size_y = IR_LUMINANCE_WORKGROUP_SIZE, local_size_z = 1) in;
void main()
{
    float countForThisBin = float(s_LuminanceHistogram.Bins[(IR_LUMINANCE_BUFFER_OFFSET_HISTOGRAM + gl_LocalInvocationIndex * 4) / 4]);
    histogram[gl_LocalInvocationIndex] = uint(countForThisBin * float(gl_LocalInvocationIndex));
    
    barrier();

    for (uint histogramSampleIndex = (IR_LUMINANCE_NUM_HISTOGRAM_BINS >> 1); histogramSampleIndex > 0u; histogramSampleIndex >>= 1)
    {
        if (gl_LocalInvocationIndex < histogramSampleIndex)
        {
            histogram[gl_LocalInvocationIndex] += histogram[gl_LocalInvocationIndex + histogramSampleIndex];
        }

        barrier();
    }

    if (gl_LocalInvocationIndex == 0u)
    {
		const float logMin = u_Uniforms.Params0.y;
		const float logMax = u_Uniforms.Params0.z;
		const float logRange = u_Uniforms.Params0.w;

		// Average bin of the non-black pixels (thread 0's countForThisBin is bin 0 = black pixels), mapped back to log2 luminance
		const float nonBlackPixels = max(u_Uniforms.Params1.y - countForThisBin, 1.0f);
		const float averageBin = float(histogram[0]) / nonBlackPixels - 1.0f;
		const float averageLog = clamp(averageBin / float(IR_LUMINANCE_NUM_HISTOGRAM_BINS - 2), 0.0f, 1.0f) * logRange + logMin;

		// [3] Previous value; the comparisons are false for NaN so they also reject it
		const float lastLuminance = uintBitsToFloat(s_LuminanceHistogram.Bins[IR_LUMINANCE_BUFFER_OFFSET_LUMINANCE / 4]);
		const bool lastIsValid = lastLuminance > 0.0f && lastLuminance < 3.0e38f;
		const float lastLog = lastIsValid ? clamp(log2(lastLuminance), logMin, logMax) : averageLog;

		// [1] + [2] Frame rate independent exponential smoothing in stops
		const float dt = max(u_Uniforms.Params1.w, 0.0f);
		const float blend = 1.0f - exp(-dt * u_Uniforms.Params0.x);
		const float adaptedLog = clamp(mix(lastLog, averageLog, blend), logMin, logMax); // [4]
		const float adaptedLuminance = exp2(adaptedLog);

		s_LuminanceHistogram.Bins[IR_LUMINANCE_BUFFER_OFFSET_LUMINANCE / 4] = floatBitsToUint(adaptedLuminance);
		s_LuminanceHistogram.Bins[IR_LUMINANCE_BUFFER_OFFSET_EXPOSURE / 4] = floatBitsToUint(u_Uniforms.Params1.z / adaptedLuminance);
    }

    // Clear the histogram for the next frame
    s_LuminanceHistogram.Bins[(IR_LUMINANCE_BUFFER_OFFSET_HISTOGRAM + gl_LocalInvocationIndex * 4) / 4] = 0u;
}
