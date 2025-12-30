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
	uint32_t albedo;
	uint32_t normal;
	uint32_t metallic_roughness;
};

[vk_binding(1)] StructuredBuffer<PrimitiveInstance> _instances;

float3 reconstructNormal(float2 xy) {
	float z = sqrt(max(0.0, 1.0 - xy.x * xy.x - xy.y * xy.y));
	return normalize(float3(xy, z));
}

[shader("fragment")] FragmentOutput main(VertexOutput input) {
	FragmentOutput output;

	PrimitiveInstance instance = _instances[uPrimitive.index];

	output.color = _staticTextures[instance.albedo].Sample(
		_samplers[SamplerType::LinearClamp], input.texcoord
	);
	output.world_space = input.frag_position;

	float3 normal = reconstructNormal(
		_staticTextures[instance.normal]
			.Sample(_samplers[SamplerType::LinearClamp], input.texcoord)
			.rg
	);
	float3x3 TBN = float3x3(input.tangent, input.bitangent, -input.normal);
	float3 localNormal = normalize(normal * 2.0 - 1.0);
	float3 transformedNormal = mul(localNormal, TBN);
	output.normal = float4(transformedNormal, 1);
	output.metallic_roughness =
		_staticTextures[instance.metallic_roughness]
			.Sample(_samplers[SamplerType::LinearClamp], input.texcoord)
			.gb;

	return output;
};
