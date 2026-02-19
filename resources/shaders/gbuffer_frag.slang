#include "base_transform.slang"
#include "common/geometry.slang"
#include "common/static_textures.slang"

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

[vk_binding(1)] StructuredBuffer<PrimitiveInstance> _materialData;
[vk_binding(2)] StructuredBuffer<uint32_t> _materialMap;
[vk_binding(3)] StructuredBuffer<uint32_t> _drawPrimitiveMap;

float3 reconstructNormal(float2 xy) {
	float z = sqrt(max(0.0, 1.0 - xy.x * xy.x - xy.y * xy.y));
	return normalize(float3(xy, z));
}

[shader("fragment")] FragmentOutput main(VertexOutput input) {
	FragmentOutput output;

	PrimitiveInstance instance = _materialData[_materialMap[_drawPrimitiveMap[input.drawIndex]]];

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
			.gb *
		float2(instance.roughness_value, instance.metallic_value);

	return output;
};
