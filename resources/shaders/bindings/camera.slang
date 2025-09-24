#pragma once



[[vk::binding(0, 0)]]
cbuffer Camera {
	float4x4 view;
	float4x4 projection;
	float4 frustumPoints[4][4];
} uCamera;