#include "common/geometry.slang"
#include "common/lighting.slang"
#include "comp/shadow.slang"
#include "comp/utils.slang"

[vk_binding(0)] ConstantBuffer<LightData> _lightData;
[vk_binding(1)] ConstantBuffer<Camera> _camera;
[vk_binding(2)] StructuredBuffer<uint> _primitives;
[vk_binding(3)] StructuredBuffer<float4x4> _transforms;
[vk_push_constant] uint32_t _cascade;

[shader("vertex")] float4 main_vs(VertexInput input, uint drawID : SV_DrawIndex) : SV_Position {
	return mul(
		mul(mul(_lightData.cascadeProjections[_cascade], _lightData.view), _transforms[_primitives[drawID]]),
		float4(input.position, 1.0)
	);
}

[shader("fragment")] void main_fs() {}
