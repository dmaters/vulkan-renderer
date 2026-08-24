#include "common/geometry.slang"
#include "common/lighting.slang"
#include "comp/shadow.slang"
#include "comp/utils.slang"

[vk_binding(0)] ConstantBuffer<LightData> _lightData;
[vk_binding(1)] ConstantBuffer<Camera> _camera;
[vk_push_constant] uint32_t _cascade;

[shader("vertex")] float4 main_vs(VertexInput input) : SV_Position {
	return mul(
		mul(mul(_lightData.cascadeProjections[_cascade], _lightData.view), input.transformation),
		float4(input.position, 1.0)
	);
}

[shader("fragment")] void main_fs() {}
