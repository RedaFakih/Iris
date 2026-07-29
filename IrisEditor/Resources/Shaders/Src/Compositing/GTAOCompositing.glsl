#version 450 core
#stage vertex

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_TexCoord;
void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position.xy, 0.0f, 1.0f);
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Occlusion;

layout(location = 0) in vec2 v_TexCoord;
layout(set = 2, binding = 0) uniform usampler2D u_GTAOTexture;

#define XE_GTAO_OCCLUSION_TERM_SCALE (1.5f) // for packing in UNORM (because raw, pre-denoised occlusion term can overshoot 1 but will later average out to 1)

void main()
{
    float occlusion = 1.0f;
    float ao = (texture(u_GTAOTexture, v_TexCoord).x >> 24) / 255.0f;
    occlusion = min(ao * XE_GTAO_OCCLUSION_TERM_SCALE, 1.0f);

    o_Occlusion = occlusion.xxxx;
}

