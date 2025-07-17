
struct Light {
	column_major float4x4 trasformation;
	float3 color;
	float intensity;
};


[[vk::binding(1, 0)]] cbuffer LightData {
	uint LIGHT_COUNT;
	Light[256] LIGHT_DATA;
};
