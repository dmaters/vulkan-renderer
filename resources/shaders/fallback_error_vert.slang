#include "bindings/geometry.slang"


struct VertexOutput {
    float4 position : SV_POSITION;
};


[[vk::binding(0, 0)]]
cbuffer ViewProj  {
	float4x4 view;
	float4x4 projection;
};
[vk::push_constant] cbuffer ModelTransform {
    float4x4 u_modelTransform;
};


[shader("vertex")]
VertexOutput main(VertexInput input) {
	VertexOutput output;
	float4x4 mvp = mul(mul(projection, view), u_modelTransform);
    output.position = mul(mvp, float4(input.position, 1.0));
	return output;
}

