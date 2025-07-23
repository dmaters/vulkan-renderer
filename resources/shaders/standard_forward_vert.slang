#include "standard_forward.slang"
#include "bindings/view_proj.slang"
#include "bindings/geometry.slang"


[shader("vertex")]
VertexOutput main(VertexInput input) {
	VertexOutput output;
	float4x4 mvp = mul(mul(PROJECTION,VIEW), MODEL);
    output.position = mul(mvp, float4(input.position, 1.0));

	output.fragPos = mul( MODEL,float4(input.position, 1.0));
	output.fragNormal = input.normal;
	output.fragTexCoord = input.texCoord;

	return output;
}