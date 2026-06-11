#version 450 core
#stage compute

#define IR_MAX_MIP_BATCH_SIZE 4

layout(set = 3, binding = 0, r32f) writeonly uniform image2D o_HierarchalZBuffer[4];

layout(set = 3, binding = 1) uniform sampler2D u_InputDepthMap;

layout(push_constant) uniform Uniforms
{
	vec2 DispatchThreadIdToBufferUV;
	vec2 InputViewportMaxBound;
	vec2 InvSize;
	uint FirstLOD;
	uint ValidMipCount;
	bool IsFirstPass;
} u_Uniforms;

shared float s_SharedClosestDeviceZ[IR_HZB_COMPUTE_WORKGROUP_SIZE * IR_HZB_COMPUTE_WORKGROUP_SIZE];

vec4 Gather4(sampler2D tex, vec2 bufferUV, float lod)
{
	vec2 uv[4];
	uv[0] = min(bufferUV + vec2(-0.5f, -0.5f) * u_Uniforms.InvSize, u_Uniforms.InputViewportMaxBound);
	uv[1] = min(bufferUV + vec2( 0.5f, -0.5f) * u_Uniforms.InvSize, u_Uniforms.InputViewportMaxBound);
	uv[2] = min(bufferUV + vec2(-0.5f,  0.5f) * u_Uniforms.InvSize, u_Uniforms.InputViewportMaxBound);
	uv[3] = min(bufferUV + vec2( 0.5f,  0.5f) * u_Uniforms.InvSize, u_Uniforms.InputViewportMaxBound);

	vec4 depth;
	depth.x = textureLod(tex, uv[0], lod).r;
	depth.y = textureLod(tex, uv[1], lod).r;
	depth.z = textureLod(tex, uv[2], lod).r;
	depth.w = textureLod(tex, uv[3], lod).r;
	
	return depth;
}

uint SignedRightShift(uint x, const int bitshift)
{
	if (bitshift > 0)
	{
		return x << uint(bitshift);
	}
	else if (bitshift < 0)
	{
		return x >> uint(-bitshift);
	}

	return x;
}

ivec2 InitialTilePixelPositionForReduction2x2(const uint tileSizeLog2, uint sharedArrayId)
{
	uint x = 0u;
	uint y = 0u;
	
	for (uint i = 0u; i < tileSizeLog2; i++)
	{
		const uint destBitId = tileSizeLog2 - 1u - i;
		const uint destBitMask = 1u << destBitId;

		x |= destBitMask & SignedRightShift(sharedArrayId, int(destBitId) - int(i * 2u));
		y |= destBitMask & SignedRightShift(sharedArrayId, int(destBitId) - int(i * 2u + 1u));
	}

	return ivec2(x, y);
}

layout(local_size_x = IR_HZB_COMPUTE_WORKGROUP_SIZE, local_size_y = IR_HZB_COMPUTE_WORKGROUP_SIZE, local_size_z = 1) in;
void main()
{
	ivec2 groupThreadId = InitialTilePixelPositionForReduction2x2(uint(IR_MAX_MIP_BATCH_SIZE - 1), gl_LocalInvocationIndex);
	ivec2 dispatchThreadId = ivec2(IR_HZB_COMPUTE_WORKGROUP_SIZE * gl_WorkGroupID) + groupThreadId;
	
	vec2 bufferUV = (vec2(dispatchThreadId) + vec2(0.5f)) * u_Uniforms.DispatchThreadIdToBufferUV.xy;

	float closestDeviceZ;
	ivec2 outputPixelPos = dispatchThreadId;

	if (u_Uniforms.IsFirstPass)
	{
		// Here we just fetch the center of the pixel since 1 thread = 1 pixel in this case since it is the first pass
		closestDeviceZ = textureLod(u_InputDepthMap, bufferUV, 0).r;
		imageStore(o_HierarchalZBuffer[0], outputPixelPos, vec4(closestDeviceZ));
	}
	else 
	{
		vec4 deviceZ = Gather4(u_InputDepthMap, bufferUV, u_Uniforms.FirstLOD - 1);

		closestDeviceZ = min(min(deviceZ.x, deviceZ.y), min(deviceZ.z, deviceZ.w));;
		imageStore(o_HierarchalZBuffer[0], outputPixelPos, vec4(closestDeviceZ));
	}

	s_SharedClosestDeviceZ[gl_LocalInvocationIndex] = closestDeviceZ;

	barrier();

	for (int mipLevel = 1; mipLevel < u_Uniforms.ValidMipCount; mipLevel++)
	{
		const int tileSize = IR_HZB_COMPUTE_WORKGROUP_SIZE / (1 << mipLevel);
		const int reduceBankSize = tileSize * tileSize;
		
		if (gl_LocalInvocationIndex < reduceBankSize)
		{
			vec4 parentDeviceZ;
			parentDeviceZ.x = closestDeviceZ;
		
			for (uint i = 1u; i < IR_MAX_MIP_BATCH_SIZE; i++)
			{
				// LDS Index
				parentDeviceZ[i] = s_SharedClosestDeviceZ[gl_LocalInvocationIndex + i * reduceBankSize];
			}
		
			closestDeviceZ = min(min(parentDeviceZ.x, parentDeviceZ.y), min(parentDeviceZ.z, parentDeviceZ.w));
			outputPixelPos = outputPixelPos >> ivec2(1);
			s_SharedClosestDeviceZ[gl_LocalInvocationIndex] = closestDeviceZ;
		
			imageStore(o_HierarchalZBuffer[mipLevel], outputPixelPos, closestDeviceZ.xxxx);
		}
	
		barrier();
	}
}