#pragma once

[vk::push_constant] cbuffer PrimitiveData {
    float4x4 transformation;
	uint32_t index;
} uPrimitive;

struct VertexInput {
	float3 position : POSITION;
	float3 normal : NORMAL;
    float3 tangent : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};
