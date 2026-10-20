#version 450 core
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;
void main()
{
	Out.Color = inColor;
	Out.UV = inUV;
	gl_Position = inPosition;
}
