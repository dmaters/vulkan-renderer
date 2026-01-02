#pragma once
#include "comp/tonemapping.slang"

[vk_binding(0)] Texture2D _base;
[vk_binding(1)] WTexture2D<float4> _output;

float3 linearToSRGB(float3 c) {
	float3 srgbLo = c * 12.92;
	float3 srgbHi = 1.055 * pow(c, float3(1.0 / 2.4)) - 0.055;
	return lerp(srgbHi, srgbLo, step(c, float3(0.0031308)));
}

void main(uint3 dispatchThreadID : SV_GroupID) {
	uint2 coord = dispatchThreadID.xy;

	float3 hdrColor = _base.Load(int3(coord, 0)).rgb;
	float3 mapped = applyToneMapping(hdrColor);
	mapped = linearToSRGB(mapped);
	_output.Store(coord, float4(mapped, 1.0));
}
