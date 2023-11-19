#version 450 core
#stage vertex

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;

layout(std140, set = 0, binding = 0) uniform TransformUniformBuffer
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
} u_TransformData;

// Bruh testing comments
/*
Whatever just testing the comments
*/

layout(location = 0) out vec3 v_Color;

void main()
{
    v_Color = a_Color;
    gl_Position = u_TransformData.Projection * (u_TransformData.View * (u_TransformData.Model * vec4(a_Position, 1.0f)));
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;
layout(location = 0) in vec3 v_Color;

void main()
{
    o_Color = vec4(v_Color, 1.0f);
}