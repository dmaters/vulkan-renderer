#pragma once


[[vk::binding(2, 0)]] cbuffer LightData {
	float4x4 view;
	float3 color;
	float intensity;
} uLightData;
