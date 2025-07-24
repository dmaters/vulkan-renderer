#pragma once


struct Light {
	column_major float4x4 trasformation;
	float3 color;
	float intensity;
};

const float4x4 _LIGHT_DIRECT_PROJECTION = {
    {2/(SCENE_SIZE * 2),0,0,0},
    {0,2/(SCENE_SIZE * 2),0,0},
    {0,0,-2/(SCENE_SIZE * 2),2},
    {0,0,0,1},
};


[[vk::binding(1, 0)]] cbuffer LightData {
	uint LIGHT_COUNT;
	uint LIGHT_DIRECT_INDEX;
	Light[256] LIGHT_DATA;
};
