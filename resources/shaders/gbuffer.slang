#include "common/geometry.slang"
#include "common/static_textures.slang"
#include "common/camera.slang"

struct VertexOutput {
	float4 position : SV_Position;
	float4 frag_position : TEXCOORD0;
	float3 normal : TEXCOORD1;
	float3 tangent : TEXCOORD2;
	float3 bitangent : TEXCOORD3;
	float2 texcoord : TEXCOORD4;
	uint drawIndex;
};


[vk_binding(0)] ConstantBuffer<Camera> _camera;

[shader("vertex")] VertexOutput main_vs(VertexInput input, uint32_t drawIndex : SV_DrawIndex) {
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


[vk_binding(1)] StructuredBuffer<PrimitiveInstance> _materialData;
[vk_binding(2)] StructuredBuffer<uint32_t> _materialMap;
[vk_binding(3)] StructuredBuffer<uint32_t> _drawPrimitiveMap;


struct FragmentOutput {
	float4 color;
	float4 world_space;
	float4 normal;
	float2 metallic_roughness;
};

struct PrimitiveInstance {
	uint32_t albedo_texture;
	uint32_t normal_texture;
	uint32_t metallic_roughness_texture;
	float metallic_value;
	float3 albedo_value;
	float roughness_value;
};


float3 reconstructNormal(float2 xy) {
	float z = sqrt(max(0.0, 1.0 - xy.x * xy.x - xy.y * xy.y));
	return normalize(float3(xy, z));
}

[shader("fragment")] FragmentOutput main_fs_alpha(VertexOutput input) {
    return output<true>(input,_drawPrimitiveMap[input.drawIndex]);
}
[shader("fragment")] FragmentOutput main_fs(VertexOutput input) {
    return output<false>(input,_drawPrimitiveMap[input.drawIndex]);
}

FragmentOutput output<bool ALPHA_TEST>(VertexOutput input, uint primitiveID) {
	FragmentOutput output;

	PrimitiveInstance instance = _materialData[_materialMap[primitiveID]];

	if(ALPHA_TEST)
       clip(
            _staticTextures[instance.albedo_texture].Sample(
            _samplers[SamplerType::LinearClamp], input.texcoord).w - 0.5
       );

	output.color = _staticTextures[instance.albedo_texture].Sample(
					   _samplers[SamplerType::LinearClamp], input.texcoord
				   ) *
	               float4(instance.albedo_value, 1);
	output.world_space = input.frag_position;

	float3 normal = reconstructNormal(
		_staticTextures[instance.normal_texture]
			.Sample(_samplers[SamplerType::LinearClamp], input.texcoord)
			.rg
	);
	float3x3 TBN = float3x3(input.tangent, input.bitangent, -input.normal);
	float3 localNormal = normalize(normal * 2.0 - 1.0);
	float3 transformedNormal = mul(localNormal, TBN);
	output.normal = float4(transformedNormal, 1);
	output.metallic_roughness =
		_staticTextures[instance.metallic_roughness_texture]
			.Sample(_samplers[SamplerType::LinearClamp], input.texcoord)
			.rg *
		float2(instance.roughness_value, instance.metallic_value);

	return output;
};
