#pragma once

struct Light {
	float4x4 view	;
	float3 color;
	float intensity;
};


[[vk::binding(2, 0)]] cbuffer LightData {
	uint count;
	uint directIndex;
	Light[256] lights;
} uLightData;
