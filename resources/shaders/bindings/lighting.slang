#pragma once

struct Light {
	float4x4 trasformation;
	float3 color;
	float intensity;
};

constexpr float4x4 directLightProjectionMatrix(float frustumSize) {
	return float4x4(
		1/(frustumSize),0,0,0,
		0,1/frustumSize,0,0,
		0,0,-1/(frustumSize*2), 0,
		0,0,0,1,
	);
}


[[vk::binding(2, 0)]] cbuffer LightData {
	uint count;
	uint directIndex;
	Light[256] lights;
} uLightData;
