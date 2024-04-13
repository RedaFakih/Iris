#version 450 core
#stage vertex

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;

// Transforms Buffer
layout(location = 5) in vec4 a_MatrixRow0;
layout(location = 6) in vec4 a_MatrixRow1;
layout(location = 7) in vec4 a_MatrixRow2;

layout(std140, set = 1, binding = 0) uniform Camera
{
	mat4 ViewProjectionMatrix;
	mat4 InverseViewProjectionMatrix;
	mat4 ProjectionMatrix;
	mat4 InverseProjectionMatrix;
	mat4 ViewMatrix;
	mat4 InverseViewMatrix;
	vec2 DepthUnpackConsts;
} u_Camera;

struct VertexOutput
{
	vec3 WorldPosition;
	vec3 Normal;
	vec2 TexCoord;
	mat3 WorldNormals;
	mat3 WorldTransform;
	vec3 Binormal;

	mat3 CameraView;
	vec3 ViewPosition;
};

layout(location = 0) out VertexOutput Output;

// Make sure both the PreDdepth shader and the PBR shader compute the exact same result
invariant gl_Position;

void main()
{
	mat4 transform = mat4(
		vec4(a_MatrixRow0.x, a_MatrixRow1.x, a_MatrixRow2.x, 0.0),
		vec4(a_MatrixRow0.y, a_MatrixRow1.y, a_MatrixRow2.y, 0.0),
		vec4(a_MatrixRow0.z, a_MatrixRow1.z, a_MatrixRow2.z, 0.0),
		vec4(a_MatrixRow0.w, a_MatrixRow1.w, a_MatrixRow2.w, 1.0)
	);

    vec4 worldPosition = transform * vec4(a_Position, 1.0f);

    Output.WorldPosition = worldPosition.xyz;
    Output.Normal = mat3(transform) * a_Normal;
    Output.TexCoord = vec2(a_TexCoord.x, 1.0f - a_TexCoord.y); // Invert y texture coordinate
    Output.WorldNormals = mat3(transform) * mat3(a_Tangent, a_Binormal, a_Normal);
    Output.WorldTransform = mat3(transform);
	Output.Binormal = a_Binormal;
    
    Output.CameraView = mat3(u_Camera.ViewMatrix);
    Output.ViewPosition = vec3(u_Camera.ViewMatrix * vec4(Output.WorldPosition, 1.0f));

    gl_Position = u_Camera.ViewProjectionMatrix * worldPosition;
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;
layout(location = 1) out vec4 o_Normals;
layout(location = 2) out vec4 o_Roughness;

struct VertexOutput
{
	vec3 WorldPosition;
	vec3 Normal;
	vec2 TexCoord;
	mat3 WorldNormals;
	mat3 WorldTransform;
	vec3 Binormal;

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
vec3 s_LightPos = vec3(-20.2f, 6.0f, -5.0f);

void main()
{
    // Ambient since we do not have IBL
    float ambientStrength = 0.2f;
    vec4 ambient = vec4(ambientStrength * s_LightColor, 1.0f);

    vec3 norm = normalize(Input.Normal);
    vec3 lightDir = normalize(s_LightPos - Input.WorldPosition);
    
    float diff = max(dot(norm, lightDir), 0.0f);
    vec4 diffuse = vec4(diff * s_LightColor, 1.0f);

    // o_Color = vec4(vec3(texture(u_MetalnessTexture, Input.TexCoord).b), 1.0f);
    o_Color = (ambient + diffuse) * (texture(u_AlbedoTexture, Input.TexCoord) * vec4(u_MaterialUniforms.AlbedoColor, 1.0f));
    o_Normals = vec4(texture(u_NormalTexture, Input.TexCoord).rgb, 1.0f);
	o_Roughness = vec4(vec3(texture(u_RoughnessTexture, Input.TexCoord).g), 1.0f);
}