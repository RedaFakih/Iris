#version 450 core
#stage vertex

// The MIT License
// Copyright © 2017 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// Analiyically filtering a grid pattern (ie, not using supersampling or mipmapping.
//
// Info: https://iquilezles.org/articles/filterableprocedurals
//  
// More filtered patterns:  https://www.shadertoy.com/playlist/l3KXR1

/*
 * TODO: This is a shader that raycasts an infinite grid with a bit of fading.
 * However there is a problem that it is not fixed and the grid moves with the camera...
 */

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_Position;
layout(location = 1) out vec3 v_NearPoint;
layout(location = 2) out vec3 v_FarPoint;

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

vec3 UnProjectPoint(vec3 pos)
{
	vec4 unProjectedPoint = u_Camera.InverseViewMatrix * u_Camera.InverseProjectionMatrix * vec4(pos.xyz, 1.0f);
	return unProjectedPoint.xyz / unProjectedPoint.w;
}

void main()
{
	v_Position = a_Position.xy;
	v_NearPoint = UnProjectPoint(vec3(a_Position.xy, 1.0f));
	v_FarPoint = UnProjectPoint(vec3(a_Position.xy, 0.0f));

	// Silence unused attribute warning...
	gl_Position = vec4(a_TexCoord, 0.0f, 1.0f);	
	gl_Position = vec4(a_Position, 1.0f);
}

#version 450 core
#stage fragment

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec2 v_Position;
layout(location = 1) in vec3 v_NearPoint;
layout(location = 2) in vec3 v_FarPoint;

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

layout(push_constant) uniform Uniforms
{
	vec4 LookAt;
	float Scale;
} u_Uniforms;

// --- analytically box-filtered grid ---

float GridTextureGradBox(vec2 p, vec2 ddx, vec2 ddy)
{
	// filter kernel
    vec2 w = max(abs(ddx), abs(ddy)) + 0.01f;

	// analytic (box) filtering
    vec2 a = p + 0.5f * w;                        
    vec2 b = p - 0.5f * w;           
    vec2 i = (floor(a) + min(fract(a) * u_Uniforms.Scale, 1.0f) - floor(b) - min(fract(b) * u_Uniforms.Scale, 1.0f)) / (u_Uniforms.Scale * w);
    //pattern
    return (1.0f - i.x) * (1.0f - i.y);
}

float Intersect(vec3 ro, vec3 rd, out vec3 pos, out vec3 nor, out int matid)
{
    // raytrace
	float tmin = 10000.0f;
	nor = vec3(0.0f);
	pos = vec3(0.0f);
    matid = -1;
	
	// raytrace-plane
	float h = (0.01f - ro.y) / rd.y;
	if (h > 0.0f) 
	{ 
		tmin = h; 
		nor = vec3(0.0f, 1.0f, 0.0f); 
		pos = ro + h * rd;
		matid = 0;
	}

	return tmin;	
}

vec2 TexCoords(vec3 pos, int mid)
{
    vec2 matuv;
    
    if (mid == 0)
    {
        matuv = pos.xz;
    }

	return 1.0f * matuv;
}

void CalcRayForPixel(vec2 pix, out vec3 resRo, out vec3 resRd)
{
	vec3 ro, ta;
	ro = vec3(u_Camera.InverseViewMatrix[3]);
	// create view ray
	vec3 lookAt = ro + u_Uniforms.LookAt.xyz;
	vec3 ww = normalize(lookAt - ro);
	mat3 viewMatrix = mat3(transpose(u_Camera.ViewMatrix));
	vec3 rd = normalize(pix.x * viewMatrix[0] + pix.y * viewMatrix[1] + 2.0f * ww);
	
	resRo = ro;
	resRd = rd;
}

float ComputeDepth(vec3 pos)
{
	vec4 clipSpacePos = u_Camera.ViewProjectionMatrix * vec4(pos, 1.0f);
	return clipSpacePos.z / clipSpacePos.w;
}

// From XeGTAO
float LinearizeDepth(const float screenDepth)
{
	float depthLinearizeMul = u_Camera.DepthUnpackConsts.x;
	float depthLinearizeAdd = u_Camera.DepthUnpackConsts.y;
	// Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) * cameraClipFar"
	return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

void main()
{    
	vec3 ro, rd, ddx_ro, ddx_rd, ddy_ro, ddy_rd;
	CalcRayForPixel(v_Position, ro, rd);
	CalcRayForPixel(v_Position, ddx_ro, ddx_rd);
	CalcRayForPixel(v_Position, ddy_ro, ddy_rd);
		
    // trace
	vec3 pos, nor;
    int mid;
    float t = Intersect(ro, rd, pos, nor, mid);

	if (mid != -1)
	{
		float t2 = (-v_NearPoint.y) / (v_FarPoint.y - v_NearPoint.y);
		vec3 fragPos = v_NearPoint + t2 * (v_FarPoint - v_NearPoint);

		float fragmentDepth = ComputeDepth(fragPos);

		gl_FragDepth = fragmentDepth;

		float linearDepth = 1.0f / LinearizeDepth(fragmentDepth);
		float fading = max(0.0f, linearDepth);

		// -----------------------------------------------------------------------
        // compute ray differentials by intersecting the tangent plane to the  
        // surface.		
		// -----------------------------------------------------------------------

		// computer ray differentials
		vec3 ddx_pos = ddx_ro - ddx_rd * dot(ddx_ro - pos, nor) / dot(ddx_rd, nor);
		vec3 ddy_pos = ddy_ro - ddy_rd * dot(ddy_ro - pos, nor) / dot(ddy_rd, nor);

		// calc texture sampling footprint		
		vec2 uv = TexCoords(pos, mid);
		vec2 ddx_uv = TexCoords(ddx_pos, mid) - uv;
		vec2 ddy_uv = TexCoords(ddy_pos, mid) - uv;
        
		// shading		
		o_Color = 1.0f - (vec4(1.0f) * GridTextureGradBox(uv, ddx_uv, ddy_uv));
		o_Color.rgb = -o_Color.rgb;
		o_Color.a *= fading;
	}
	else
		discard;
}