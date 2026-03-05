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


[shader("vertex")] float4 main_vs(VertexInput input) : SV_Position {
	return mul(
		mul(mul(_lightData.cascadeProjections[_shadowData.cascade],
	            _lightData.view),
	        input.transformation),
		float4(input.position, 1.0)
	);
}


struct VertexOutputAlphaTested {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    uint drawID;
};

[shader("vertex")] VertexOutputAlphaTested main_vs_alpha(VertexInput input, uint drawID : SV_DrawIndex) {
	VertexOutputAlphaTested output;


	output.position = mul(
		mul(mul(_lightData.cascadeProjections[_shadowData.cascade],
	            _lightData.view),
	        input.transformation),
		float4(input.position, 1.0)
	);

	output.texcoord = input.texCoord;
	output.drawID = drawID;
	return output;
}



struct PrimitiveInstance {
	uint32_t albedo_texture;
	uint32_t normal_texture;
	uint32_t metallic_roughness_texture;
	float metallic_value;
	float3 albedo_value;
	float roughness_value;
};

[vk_binding(2)] StructuredBuffer<PrimitiveInstance> _materialData;
[vk_binding(3)] StructuredBuffer<uint32_t> _materialMap;
[vk_binding(4)] StructuredBuffer<uint32_t> _primitives;





[shader("fragment")] void main_fs() {}
[shader("fragment")] void main_fs_alpha(VertexOutputAlphaTested input) {
    PrimitiveInstance instance = _materialData[_materialMap[_primitives[input.drawID]]];

    clip(
        _staticTextures[instance.albedo_texture].Sample(
        _samplers[SamplerType::LinearClamp], input.texcoord).w - 0.5
    );
}
