#pragma once


[vk::push_constant] cbuffer PrimitiveData {
    float4x4 MODEL;
	uint32_t PRIMITIVE_INDEX;
};

struct VertexInput {
	float3 position : POSITION;
	float3 normal : NORMAL;
    float3 tangent : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};
