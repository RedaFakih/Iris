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

// Should be in set 3
layout(set = 0, binding = 0) uniform sampler2D u_Texture;

void main()
{
//    mat3 kernel;
    // Edge detection
//    kernel[0] = vec3(0.0f, -1.0f, 0.0f);
//    kernel[1] = vec3(-1.0f, 4.0f, -1.0f);
//    kernel[2] = vec3(0.0f, -1.0f, 0.0f);

    // Sharpen
//    kernel[0] = vec3(0.0f, -1.0f, 0.0f);
//    kernel[1] = vec3(-1.0f, 5.0f, -1.0f);
//    kernel[2] = vec3(0.0f, -1.0f, 0.0f);

    // Guassian blur
//    kernel[0] = vec3(1.0f, 2.0f, 1.0f);
//    kernel[1] = vec3(2.0f, 4.0f, 2.0f);
//    kernel[2] = vec3(1.0f, 2.0f, 1.0f);
//    kernel /= 16;

//    vec3 sum = vec3(0.0);
//    ivec2 texSize = textureSize(u_Texture, 0);
    // Iterate over the 3x3 kernel
//    for (int i = -1; i <= 1; ++i) {
//        for (int j = -1; j <= 1; ++j) {
//            vec2 offset = vec2(i, j);
//            vec3 sample_ = texture(u_Texture, v_TexCoord + offset / texSize).rgb;
//            sum += kernel[i+1][j+1] * sample_;
//        }
//    }

    // Output the result
//    vec4 FragColor = vec4(sum, 1.0);
//    o_Color = FragColor;

    o_Color = texture(u_Texture, v_TexCoord);
}