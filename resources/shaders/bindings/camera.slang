#pragma once

[[vk::binding(0, 0)]]
cbuffer Camera {
	float4x4 view;
	float4x4 projection;
} uCamera;