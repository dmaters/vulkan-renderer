#include "common/camera.slang"
#include "common/geometry.slang"
#include "common/primitive.slang"
#include "common/static_textures.slang"

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
	float4x4 mvp = mul(mul(_camera.projection, _camera.view), input.transformation);
	output.position = mul(mvp, float4(input.position, 1.0));

	output.frag_position = mul(input.transformation, float4(input.position, 1.0));
	output.normal = mul(input.transformation, float4(input.normal, 1)).xyz;
	output.texcoord = input.texCoord;
	output.tangent = mul(input.transformation, float4(input.tangent, 1)).xyz;
	output.bitangent = mul(input.transformation, float4(input.bitangent, 1)).xyz;
	output.drawIndex = drawIndex;
	return output;
}

struct PBRmaterial {
	uint32_t albedo_texture;
	uint32_t normal_texture;
	uint32_t metallic_roughness_texture;
	float metallic_value;
	float3 albedo_value;
	float roughness_value;
};

[vk_binding(1)] StructuredBuffer<PBRmaterial> _materialData;
[vk_binding(2)] StructuredBuffer<PrimitiveData> _primitiveData;
[vk_binding(3)] StructuredBuffer<uint32_t> _primitiveIndices;

struct FragmentOutput {
	float4 color;
	float4 world_space;
	float4 normal;
	float2 metallic_roughness;
};

float3 reconstructNormal(float2 xy) {
	float z = sqrt(1 - dot(xy, xy));
	return normalize(float3(xy, z));
}

[shader("fragment")] FragmentOutput main_fs(VertexOutput input) {
	FragmentOutput output;
	PrimitiveData primitiveData = _primitiveData[_primitiveIndices[input.drawIndex]];

	PBRmaterial material = _materialData[primitiveData.materialIndex];

	clip(_staticTextures[material.albedo_texture].Sample(_samplers[SamplerType::LinearRepeat], input.texcoord).w - 0.5);

	output.color =
		_staticTextures[material.albedo_texture].Sample(_samplers[SamplerType::LinearRepeat], input.texcoord) *
		float4(material.albedo_value, 1);
	output.world_space = input.frag_position;

	float2 sampledNormal =
		_staticTextures[material.normal_texture].Sample(_samplers[SamplerType::LinearRepeat], input.texcoord).rg;
	float2 localNormal = sampledNormal * 2.0 - 1.0;
	float3 normal = reconstructNormal(localNormal);
	float3x3 TBN = transpose(float3x3(input.tangent, input.bitangent, input.normal));
	float3 transformedNormal = normalize(mul(TBN, normal));

	output.normal = float4(transformedNormal, 1);
	output.metallic_roughness = _staticTextures[material.metallic_roughness_texture]
									.Sample(_samplers[SamplerType::LinearRepeat], input.texcoord)
									.rg *
								float2(material.roughness_value, material.metallic_value);

	return output;
}
