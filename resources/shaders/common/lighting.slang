#pragma once

struct LightData {
	float4x4 view;
	float4x4 cascadeProjections[3];
	float3 color;
	float intensity;
	float4 cascadeDistances;
	float3 paddings;
};
