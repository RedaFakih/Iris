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

void main()
{
	vec4 pixel = texture(u_Texture, v_TexCoord);

	// Signed distance (squared)
	float dist = sqrt(pixel.z);
	float alpha = smoothstep(0.004f, 0.003f, dist);
	if (alpha == 0.0f)
		discard;

	vec3 outlineColor = vec3(0.14f, 0.8f, 0.52f);
	o_Color = vec4(outlineColor, alpha);
}