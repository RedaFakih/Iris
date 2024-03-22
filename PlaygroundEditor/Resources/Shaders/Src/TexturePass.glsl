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

layout(std140, set = 0, binding = 1) uniform TransformUniformBuffer
{
    mat4 Model;
    mat4 ViewProjection;
    vec2 DepthUnpackConsts;
} u_TransformData;

layout(set = 3, binding = 0) uniform sampler2D u_Texture;

float near = 0.1f;
float far = 10000.0f;

// From XeGTAO
float ScreenSpaceToViewSpaceDepth(const float screenDepth)
{
	float depthLinearizeMul = u_TransformData.DepthUnpackConsts.x;
	float depthLinearizeAdd = u_TransformData.DepthUnpackConsts.y;
	// Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) * cameraClipFar"
	return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

void main()
{
    // mat3 kernel;
    // Edge detection
    // kernel[0] = vec3(0.0f, -1.0f, 0.0f);
    // kernel[1] = vec3(-1.0f, 4.0f, -1.0f);
    // kernel[2] = vec3(0.0f, -1.0f, 0.0f);

    // Sharpen
    // kernel[0] = vec3(0.0f, -1.0f, 0.0f);
    // kernel[1] = vec3(-1.0f, 5.0f, -1.0f);
    // kernel[2] = vec3(0.0f, -1.0f, 0.0f);

    // Guassian blur
    // kernel[0] = vec3(1.0f, 2.0f, 1.0f);
    // kernel[1] = vec3(2.0f, 4.0f, 2.0f);
    // kernel[2] = vec3(1.0f, 2.0f, 1.0f);
    // kernel /= 16;

    // vec3 sum = vec3(0.0);
    // ivec2 texSize = textureSize(u_Texture, 0);
    // // Iterate over the 3x3 kernel
    // for (int i = -1; i <= 1; ++i) {
    //     for (int j = -1; j <= 1; ++j) {
    //         vec2 offset = vec2(i, j);
    //         vec3 sample_ = texture(u_Texture, v_TexCoord + offset / texSize).rgb;
    //         sum += kernel[i+1][j+1] * sample_;
    //     }
    // }

    // Output the result
    // vec4 FragColor = vec4(sum, 1.0);
    // o_Color = FragColor;

    float linearDepth = near / ScreenSpaceToViewSpaceDepth(texture(u_Texture, v_TexCoord).r);
    o_Color = vec4(vec3(linearDepth), 1.0f);
}