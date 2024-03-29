#version 450 core
#stage vertex

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;

struct VertexOutput
{
	vec3 WorldPosition;
	vec3 Normal;
	vec2 TexCoord;
	mat3 WorldNormals;
	mat3 WorldTransform;

	mat3 CameraView;
	vec3 ViewPosition;
};

layout(location = 0) out VertexOutput Output;

layout(std140, set = 0, binding = 1) uniform TransformUniformBuffer
{
    mat4 Model;
    mat4 ViewProjection;
    vec2 DepthUnpackConsts;
} u_TransformData;

void main()
{
    vec4 worldPosition = u_TransformData.Model * vec4(a_Position, 1.0f);

    Output.WorldPosition = worldPosition.xyz;
    Output.Normal = mat3(u_TransformData.Model) * a_Normal;
    Output.TexCoord = vec2(a_TexCoord.x, 1.0f - a_TexCoord.y); // Invert y texture coordinate
    Output.WorldNormals = mat3(u_TransformData.Model) * mat3(a_Tangent, a_Binormal, a_Normal);
    Output.WorldTransform = mat3(u_TransformData.Model);
    
    Output.CameraView = mat3(1.0f); // TODO: Add camera view matrix
    Output.ViewPosition = vec3(0.0f); // TODO: Add camera view matrix and multiply with world position

    gl_Position = u_TransformData.ViewProjection * worldPosition;
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;

struct VertexOutput
{
	vec3 WorldPosition;
	vec3 Normal;
	vec2 TexCoord;
	mat3 WorldNormals;
	mat3 WorldTransform;

	mat3 CameraView;
	vec3 ViewPosition;
};

layout(location = 0) in VertexOutput Input;

layout(set = 3, binding = 0) uniform sampler2D u_AlbedoTexture;
layout(set = 3, binding = 1) uniform sampler2D u_NormalTexture;
layout(set = 3, binding = 2) uniform sampler2D u_RoughnessTexture;
layout(set = 3, binding = 3) uniform sampler2D u_MetalnessTexture;

layout(push_constant) uniform Materials
{
    vec3 AlbedoColor;
    float Roughness;
    float Metalness;
    float Emission;

    bool UseNormalMap;
} u_MaterialUniforms;

vec3 s_LightColor = vec3(1.0f);
vec3 s_LightPos = vec3(1.2f, 5.0f, 2.0f);

void main()
{
    // Ambient since we do not have IBL
    float ambientStrength = 0.2f;
    vec4 ambient = vec4(ambientStrength * s_LightColor, 1.0f);

    vec3 norm = normalize(Input.Normal);
    vec3 lightDir = normalize(s_LightPos - Input.WorldPosition);
    
    float diff = max(dot(norm, lightDir), 0.0f);
    vec4 diffuse = vec4(diff * s_LightColor, 1.0f);

    // o_Color = vec4(texture(u_NormalTexture, Input.TexCoord).rgb, 1.0f);
    // o_Color = vec4(vec3(texture(u_MetalnessTexture, Input.TexCoord).b), 1.0f);
    // o_Color = vec4(vec3(texture(u_RoughnessTexture, Input.TexCoord).g), 1.0f);
    o_Color = (ambient + diffuse) * (texture(u_AlbedoTexture, Input.TexCoord) * vec4(u_MaterialUniforms.AlbedoColor, 1.0f));
}