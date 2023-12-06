#version 450 core
#stage vertex

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;
layout(location = 2) in vec2 a_TexCoord;

layout(location = 0) out vec3 v_Color;
layout(location = 1) out vec2 v_TexCoord;

layout(std140, set = 0, binding = 0) uniform TransformUniformBuffer
{
    mat4 Model;
    mat4 ViewProjection;
} u_TransformData;

// Bruh testing comments
/*
Whatever just testing the comments
*/

void main()
{
    v_Color = a_Color;
    v_TexCoord = a_TexCoord;
    gl_Position = u_TransformData.ViewProjection * (u_TransformData.Model * vec4(a_Position, 1.0f));
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec3 v_Color;
layout(location = 1) in vec2 v_TexCoord;

// Should be in set 3
layout(set = 1, binding = 0) uniform sampler2D u_Texture;

layout(push_constant) uniform Material
{
    vec4 Color;
    float Emission;
} u_MaterialInfo;

void main()
{
    o_Color = vec4(v_Color, 1.0f);
    o_Color = texture(u_Texture, v_TexCoord);
}