#pragma vertex
#version 450 core

layout(push_constant) uniform Transform
{
    float A;
    float B;
} u_Test;

layout(location = 0) out vec3 v_Color;

vec2 g_Positions[3] = {
    vec2( 0.0f, -0.5f),
    vec2( 0.5f,  0.5f),
    vec2(-0.5f,  0.5f)
};

vec3 g_Colors[3] = {
    vec3(1.0f, 0.0f, 0.0f),
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 0.0f, 1.0f)
};

void main()
{
    float testest = u_Test.A;

    v_Color = g_Colors[gl_VertexIndex];
    gl_Position = vec4(g_Positions[gl_VertexIndex], 0.0f, 1.0f);
}

#pragma fragment
#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 0) in vec3 v_Color;

void main()
{
    o_Color = vec4(v_Color, 1.0f);
}