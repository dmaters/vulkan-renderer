#pragma once

[[vk::binding(0, 0)]]
cbuffer Camera {
	float4x4 view;
	float4x4 projection;
	float4 frustumBounds[8];
	float4 frustumPlanes[6];
} uCamera;