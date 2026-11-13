import definitions.camera;
import definitions.primitive;
import definitions.pbr;


import static_textures;
import samplers;


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
[vk_binding(1)] StructuredBuffer<float4x4> _transformations;

[shader("vertex")] VertexOutput main_vs(VertexInput input, uint32_t drawIndex : SV_DrawIndex) {
	VertexOutput output;
	uint primitive = _primitiveIndices[drawIndex];
	float4x4 transform = _transformations[primitive];
	float4x4 mvp = mul(mul(_camera.projection, _camera.view), transform);
	output.position = mul(mvp, float4(input.position, 1.0));

	output.frag_position = mul(transform, float4(input.position, 1.0));
	output.normal = mul(transform, float4(input.normal, 1)).xyz;
	output.texcoord = input.texCoord;
	output.tangent = mul(transform, float4(input.tangent, 1)).xyz;
	output.bitangent = mul(transform, float4(input.bitangent, 1)).xyz;
	output.drawIndex = drawIndex;
	return output;
}



[vk_binding(2)] StructuredBuffer<Material> _materialData;
[vk_binding(3)] StructuredBuffer<PrimitiveData> _primitiveData;
[vk_binding(4)] StructuredBuffer<uint32_t> _primitiveIndices;

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

	Material material = _materialData[primitiveData.materialIndex];

	clip(_staticTextures[material.albedo_texture].Sample(_samplers[SamplerType::LinearRepeat], input.texcoord).w - 0.5);

	output.color =
		_staticTextures[material.albedo_texture].Sample(_samplers[SamplerType::LinearRepeat], input.texcoord);
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
