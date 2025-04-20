#version 450 core
#stage compute

layout(set = 3, binding = 0, rgba32f) restrict writeonly uniform image2D o_Image;

layout(set = 3, binding = 1) uniform sampler2D u_Texture;
layout(set = 3, binding = 2) uniform sampler2D u_BloomTexture;

layout(push_constant) uniform Uniforms
{
    vec4 Params; // (x) threshold, (y) threshold - knee, (z) knee * 2, (w) 0.25 / knee
    float UpsampleScale;
    float LOD;
} u_Uniforms;

vec3 UpsampleTent9(sampler2D tex, float lod, vec2 uv, vec2 texelSize, float radius)
{
    vec4 offset = texelSize.xyxy * vec4(1.0f, 1.0f, -1.0f, 0.0f) * radius;

    // Center
    vec3 result = textureLod(tex, uv, lod).rgb * 4.0f;

    result += textureLod(tex, uv - offset.xy, lod).rgb;
    result += textureLod(tex, uv - offset.wy, lod).rgb * 2.0f;
    result += textureLod(tex, uv - offset.zy, lod).rgb;

    result += textureLod(tex, uv + offset.zw, lod).rgb * 2.0f;
    result += textureLod(tex, uv + offset.xw, lod).rgb * 2.0f;

    result += textureLod(tex, uv + offset.zy, lod).rgb;
    result += textureLod(tex, uv + offset.wy, lod).rgb * 2.0f;
    result += textureLod(tex, uv + offset.xy, lod).rgb;

    return result * (1.0f / 16.0f);
}

layout(local_size_x = IR_BLOOM_COMPUTE_WORKGROUP_SIZE, local_size_y = IR_BLOOM_COMPUTE_WORKGROUP_SIZE, local_size_z = 1) in;
void main()
{
    vec2 imgSize = vec2(imageSize(o_Image));
    vec2 texSize = vec2(textureSize(u_Texture, int(u_Uniforms.LOD)));

    ivec2 invocID = ivec2(gl_GlobalInvocationID);
    vec2 texCoords = vec2(float(invocID.x) / imgSize.x, float(invocID.y) / imgSize.y);
    texCoords += (1.0f / imgSize) * 0.5f;

    vec4 color = vec4(1.0f, 0.0f, 1.0f, 1.0f);

    // Upsample
    vec2 bloomTexSize = vec2(textureSize(u_BloomTexture, int(u_Uniforms.LOD + 1.0f)));
    vec3 upsampledTexture = UpsampleTent9(u_BloomTexture, u_Uniforms.LOD + 1.0f, texCoords, 1.0f / bloomTexSize, u_Uniforms.UpsampleScale);

    vec3 existing = textureLod(u_Texture, texCoords, u_Uniforms.LOD).rgb;
    color.rgb = existing + upsampledTexture;
    
    imageStore(o_Image, ivec2(gl_GlobalInvocationID), color);
}
