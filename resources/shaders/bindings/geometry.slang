#pragma once

[vk::push_constant] cbuffer PrimitiveData {
	uint32_t index;
} uPrimitive;



struct VertexInput {
    [[vk::location(0)]]
    float3 position : POSITION;
    [[vk::location(1)]]
    float3 normal : NORMAL;
    [[vk::location(2)]]
    float3 tangent : TEXCOORD0;
    [[vk::location(3)]]
    float3 bitangent : TEXCOORD1;
    [[vk::location(4)]]
    float2 texCoord : TEXCOORD2;
    [[vk::location(5)]] 
    float4x4 transformation;
};

