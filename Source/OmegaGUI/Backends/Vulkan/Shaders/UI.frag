#version 450 core
layout(constant_id = 0) const bool kLinearOutput = false;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D sTexture;
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
void main()
{
	vec4 colOut = In.Color * texture(sTexture, In.UV.st);
	if (kLinearOutput)
		colOut.rgb = mix(colOut.rgb / 12.92, pow((colOut.rgb + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), colOut.rgb));
	outColor = colOut;
}
