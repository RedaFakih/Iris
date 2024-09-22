#version 450 core
#stage vertex

// Vertex Attributes
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

layout(set = 1, binding = 0) uniform sampler2D u_Texture;

float ScreenDistance(vec2 v, vec2 texelSize)
{
	float ratio = texelSize.x / texelSize.y;
	v.x /= ratio;
	return dot(v, v);
}

void main()
{
	vec4 color = texture(u_Texture, v_TexCoord);

	ivec2 texSize = textureSize(u_Texture, 0);
	vec2 texelSize = vec2(1.0f / float(texSize.x), 1.0f / float(texSize.y));

	vec4 result;
	result.xy = vec2(100.0f, 100.0f);
	result.z = ScreenDistance(result.xy, texelSize);
	// Inside vs. Outside?
	result.w = color.a > 0.5f ? 1.0f : 0.0f;

	o_Color = result;
}