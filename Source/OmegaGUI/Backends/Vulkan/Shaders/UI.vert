#version 450 core
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(push_constant) uniform PushConstants { mat4 matProjection; } pc;
layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;
void main()
{
	Out.Color = inColor;
	Out.UV = inUV;
	gl_Position = pc.matProjection * vec4(inPosition, 0.0, 1.0);
	gl_Position.y = -gl_Position.y;
}
