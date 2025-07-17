#include "base_transform.slang"
#include "bindings/view_proj.slang"
#include "bindings/primitive.slang"
struct VertexInput {
	float3 position : POSITION;
	float3 normal : NORMAL;
    float3 tangent : TANGENT;
	float2 texCoord : TEXCOORD;
};


[shader("vertex")]
VertexOutput main(VertexInput input) {
	VertexOutput output;
	float4x4 mvp = mul(mul(PROJECTION, VIEW),MODEL );
    output.position = mul(mvp, float4(input.position, 1.0));

	output.frag_position = mul(MODEL  ,float4(input.position, 1.0));
	output.normal = input.normal;
	output.texcoord = input.texCoord;
	output.tangent = input.tangent;
	output.bitangent = cross(input.tangent,input.normal);
	return output;
}