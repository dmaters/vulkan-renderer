import definitions.light;
import definitions.camera;
import definitions.primitive;


[vk_binding(0)] ConstantBuffer<Light> _light;
[vk_binding(1)] ConstantBuffer<Camera> _camera;
[vk_binding(2)] StructuredBuffer<uint> _primitives;
[vk_binding(3)] StructuredBuffer<float4x4> _transforms;
[vk_push_constant] uint32_t _cascade;

[shader("vertex")] float4 main_vs(VertexInput input, uint drawID : SV_DrawIndex) : SV_Position {
	return mul(
		mul(mul(_light.cascadeProjections[_cascade], _light.view), _transforms[_primitives[drawID]]),
		float4(input.position, 1.0)
	);
}

[shader("fragment")] void main_fs() {}
