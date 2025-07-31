#include "base_transform.slang"
#include "bindings/camera.slang"
#include "bindings/geometry.slang"



[shader("vertex")]
VertexOutput main(VertexInput input) {
	VertexOutput output;
	float4x4 mvp = mul(mul(uCamera.projection, uCamera.view),uPrimitive.transformation);
    output.position = mul(mvp, float4(input.position, 1.0));

	output.frag_position = mul(uPrimitive.transformation,float4(input.position , 1.0)) ;
	output.normal = input.normal;
	output.texcoord = input.texCoord;
	output.tangent = input.tangent;
	output.bitangent = cross(input.tangent,input.normal);
	return output;
}
