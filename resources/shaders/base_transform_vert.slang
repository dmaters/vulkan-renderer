#include "base_transform.slang"
#include "common/camera.slang"
#include "common/geometry.slang"

[vk_binding(0)] ConstantBuffer<Camera> _camera;
[shader("vertex")] VertexOutput main(VertexInput input, uint32_t drawIndex : SV_DrawIndex) {
	VertexOutput output;
	float4x4 mvp =
		mul(mul(_camera.projection, _camera.view), input.transformation);
	output.position = mul(mvp, float4(input.position, 1.0));

	output.frag_position =
		mul(input.transformation, float4(input.position, 1.0));
	output.normal = input.normal;
	output.texcoord = input.texCoord;
	output.tangent = input.tangent;
	output.bitangent = input.bitangent;
	output.drawIndex = drawIndex;
	return output;
}
