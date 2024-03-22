/*
 * TODO: This should be a compute shader however there is still no support for compute pipelines....
 */

#version 450 core
#stage vertex

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 1.0f);
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec2 v_TexCoord;

layout(set = 3, binding = 0) uniform sampler2D u_Texture1;
layout(set = 3, binding = 1) uniform sampler2D u_Texture2;
layout(set = 3, binding = 2) uniform sampler2D u_DepthTexture1;
layout(set = 3, binding = 3) uniform sampler2D u_DepthTexture2;

void main()
{
    // Compare current depths then decide what is the final color on the final image by sampling the appropriate color texture
    if (texture(u_DepthTexture2, v_TexCoord).r >= texture(u_DepthTexture1, v_TexCoord).r)
    {
        o_Color = vec4(texture(u_Texture1, v_TexCoord).rgb, 1.0f);
    }
    else
    {
        o_Color = vec4(texture(u_Texture2, v_TexCoord).rgb, 1.0f);
    }
}