#pragma once

struct Camera {
	float4x4 view;
	float4x4 projection;
	float4x4 invView;
	float4x4 invProj;
	float4 position;
	float4 direction;
};
