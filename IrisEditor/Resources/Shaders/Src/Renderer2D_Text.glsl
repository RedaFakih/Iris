#version 450 core
#stage vertex

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;

layout (std140, set = 0, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
};

layout (push_constant) uniform Transform
{
	mat4 Transform;
} u_Renderer;

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
};

layout (location = 0) out VertexOutput Output;
layout (location = 5) out flat float TexIndex;

void main()
{
	Output.Color = a_Color;
	Output.TexCoord = a_TexCoord;

	TexIndex = a_TexIndex;

	gl_Position = u_ViewProjection * u_Renderer.Transform * vec4(a_Position, 1.0f);
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
};

layout (location = 0) in VertexOutput Input;
layout (location = 5) in flat float TexIndex;

layout (set = 3, binding = 0) uniform sampler2D u_FontAtlases[32];

float Median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float ScreenPxRange()
{
	const float pxRange = 2.0f;
    vec2 unitRange = vec2(pxRange) / vec2(textureSize(u_FontAtlases[int(TexIndex)], 0));
    vec2 screenTexSize = vec2(1.0f) / fwidth(Input.TexCoord);
    return max(0.5f * dot(unitRange, screenTexSize), 1.0f);
}

void main()
{
	vec4 bgColor = vec4(Input.Color.rgb, 0.0f);
	vec4 fgColor = Input.Color;

	vec3 msd = texture(u_FontAtlases[int(TexIndex)], Input.TexCoord).rgb;
	float sd = Median(msd.r, msd.g, msd.b);
	float screenPxDistance = ScreenPxRange() * (sd - 0.5f);
	float opacity = clamp(screenPxDistance + 0.5f, 0.0f, 1.0f);
	o_Color = mix(bgColor, fgColor, opacity);
}