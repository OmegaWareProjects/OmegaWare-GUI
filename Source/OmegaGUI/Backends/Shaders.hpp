#pragma once

namespace OmegaGUI::Shaders
{
	inline constexpr char szVertexHLSL[] = R"(
cbuffer ProjectionBuffer : register(b0)
{
	float4x4 ProjectionMatrix;
};
struct VS_INPUT
{
	float2 pos : POSITION;
	float2 uv : TEXCOORD0;
	float4 col : COLOR0;
};
struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};
PS_INPUT main(VS_INPUT input)
{
	PS_INPUT output;
	output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));
	output.col = input.col;
	output.uv = input.uv;
	return output;
}
)";

	inline constexpr char szWorldVertexHLSL[] = R"(
struct VS_INPUT
{
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
	float4 col : COLOR0;
};
struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};
PS_INPUT main(VS_INPUT input)
{
	PS_INPUT output;
	output.pos = input.pos;
	output.col = input.col;
	output.uv = input.uv;
	return output;
}
)";

	inline constexpr char szPixelHLSL[] = R"(
struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};
sampler sampler0;
Texture2D texture0;
float4 main(PS_INPUT input) : SV_Target
{
	return input.col * texture0.Sample(sampler0, input.uv);
}
)";

	inline constexpr char szWorldVertexHLSL9[] = R"(
struct VS_INPUT
{
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
	float4 col : COLOR0;
};
struct VS_OUTPUT
{
	float4 pos : POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};
float4 HalfPixel : register(c0);
VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.pos = input.pos;
	output.pos.xy += HalfPixel.xy * input.pos.w;
	output.col = input.col;
	output.uv = input.uv;
	return output;
}
)";

	inline constexpr char szPixelHLSL9[] = R"(
sampler2D texture0 : register(s0);
float4 main(float4 col : COLOR0, float2 uv : TEXCOORD0) : COLOR
{
	return col * tex2D(texture0, uv);
}
)";

	inline constexpr char szVertexGLSL[] = R"(#version 330 core
layout (location = 0) in vec2 Position;
layout (location = 1) in vec2 UV;
layout (location = 2) in vec4 Color;
uniform mat4 ProjectionMatrix;
out vec2 FragUV;
out vec4 FragColor;
void main()
{
	FragUV = UV;
	FragColor = Color;
	gl_Position = ProjectionMatrix * vec4(Position.xy, 0.0, 1.0);
}
)";

	inline constexpr char szWorldVertexGLSL[] = R"(#version 330 core
layout (location = 0) in vec4 Position;
layout (location = 1) in vec2 UV;
layout (location = 2) in vec4 Color;
out vec2 FragUV;
out vec4 FragColor;
void main()
{
	FragUV = UV;
	FragColor = Color;
	gl_Position = Position;
}
)";

	inline constexpr char szFragmentGLSL[] = R"(#version 330 core
in vec2 FragUV;
in vec4 FragColor;
uniform sampler2D Texture;
layout (location = 0) out vec4 OutColor;
void main()
{
	OutColor = FragColor * texture(Texture, FragUV.st);
}
)";
}
