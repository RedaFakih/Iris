#version 450 core
#stage vertex

// Vertex Attributes
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

struct VertexOutput
{
	vec2 TexCoord;
	vec2 TexelSize;
	vec2 UV[9];
};

layout(location = 0) out VertexOutput Output;

layout(push_constant) uniform Uniforms
{
	vec2 TexelSize;
	int Step;
} u_Renderer;

void main()
{
	Output.TexCoord = a_TexCoord;
	Output.TexelSize = u_Renderer.TexelSize;

	vec2 dx = vec2(u_Renderer.TexelSize.x, 0.0f) * u_Renderer.Step;
	vec2 dy = vec2(0.0f, u_Renderer.TexelSize.y) * u_Renderer.Step;

	Output.UV[0] = Output.TexCoord;

	// Sample pixels in a 3x3 block
	Output.UV[1] = Output.TexCoord + dx;
	Output.UV[2] = Output.TexCoord - dx;
	Output.UV[3] = Output.TexCoord + dy;
	Output.UV[4] = Output.TexCoord - dy;
	Output.UV[5] = Output.TexCoord + dx + dy;
	Output.UV[6] = Output.TexCoord + dx - dy;
	Output.UV[7] = Output.TexCoord - dx + dy;
	Output.UV[8] = Output.TexCoord - dx - dy;
	
	gl_Position = vec4(a_Position, 1.0f);
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;

struct VertexOutput
{
	vec2 TexCoord;
	vec2 TexelSize;
	vec2 UV[9];
};

layout(location = 0) in VertexOutput Input;

layout(set = 1, binding = 0) uniform sampler2D u_Texture;

float ScreenDistance(vec2 v)
{
	float ratio = Input.TexelSize.x / Input.TexelSize.y;
	v.x /= ratio;
	return dot(v, v);
}

vec2 BoundsCheck(vec2 xy, vec2 uv)
{
	if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
		return vec2(100.0f);

	return xy;
}

void main()
{
	vec4 pixel = texture(u_Texture, Input.UV[0]);

	for (int j = 1; j <= 8; j++)
	{
		// Sample the neighbouring pixel and make sure it is on the same side as us
		vec4 n = texture(u_Texture, Input.UV[j]);
		if (n.w != pixel.w)
			n.xyz = vec3(0.0f);

		n.xy += Input.UV[j] - Input.UV[0];

		// Invalidate out of bound neighbours
		n.xy = BoundsCheck(n.xy, Input.UV[j]);

		float dist = ScreenDistance(n.xy);
		if (dist < pixel.z)
			pixel.xyz = vec3(n.xy, dist);
	}

	o_Color = pixel;
}