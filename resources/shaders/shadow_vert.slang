#include "common/geometry.slang"
#include "common/lighting.slang"
#include "comp/shadow.slang"
#include "comp/utils.slang"

[vk_binding(0)] ConstantBuffer<LightData> _lightData;
[vk_binding(1)] ConstantBuffer<Camera> _camera;

struct ShadowData {
	uint32_t cascade;
};
[vk::push_constant] ConstantBuffer<ShadowData> _shadowData;

[shader("vertex")] float4 main(VertexInput input) : SV_Position {
	return mul(
		mul(mul(_lightData.cascadeProjections[_shadowData.cascade],
	            _lightData.view),
	        input.transformation),
		float4(input.position, 1.0)
	);
}
