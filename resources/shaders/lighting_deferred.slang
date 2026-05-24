
#include "common/camera.slang"
#include "common/lighting.slang"
#include "common/static_textures.slang"
#include "comp/pbr.slang"
#include "comp/shadow.slang"

import comp.common.spherical_harmonics;
[vk::binding(0)] uniform ConstantBuffer<Camera> _camera;
[vk::binding(1)] uniform ConstantBuffer<LightData> _lightData;
[vk::binding(2)] uniform Texture2D _albedo;
[vk::binding(3)] uniform Texture2D _world_position;
[vk::binding(4)] uniform Texture2D _normal;
[vk::binding(5)] uniform Texture2D _roughness_metallic;
[vk::binding(6)] uniform Texture2D _shadow_map;
[vk::binding(7)] uniform ConstantBuffer<SphericalHarmonic> _envSH;

struct VertexOutput {
	float4 position : SV_Position;
	float2 frag_position : TEXCOORD0;
};

[shader("fragment")] float4 main(VertexOutput input) : SV_Target {
	float3 cameraPos = (_camera.view[3].xyz) * float3(1, 1, -1);
	float4 fragPos = _world_position.Sample(
		_samplers[SamplerType::LinearClamp], input.frag_position
	);

	float3 view_dir = -transpose(_camera.view)[2].xyz;
	float3 material = float3(0);

	float3 albedo =
		_albedo.Sample(_samplers[SamplerType::LinearClamp], input.frag_position)
			.rgb;
	float3 normal =
		_normal.Sample(_samplers[SamplerType::LinearClamp], input.frag_position)
			.rgb;
	float2 roughnessMetallic =
		_roughness_metallic
			.Sample(_samplers[SamplerType::LinearClamp], input.frag_position)
			.rg;

	float shadowPercentage = Shadow::shadowPercentage(
		_shadow_map, fragPos.xyz, normal.xyz, _camera, _lightData
	);
	float3 light_dir = transpose(_lightData.view)[2].xyz;
	float3 skylight = _envSH.getValue(normal);
	float3 brdf = PBR::microfacetBRDF(
		input.frag_position,
		view_dir,
		light_dir,
		albedo,
		normal,
		roughnessMetallic.x,
		roughnessMetallic.y
	);
	material += (1 - shadowPercentage) * brdf * _lightData.intensity *
	            _lightData.color;
	material += skylight * 2.7;  // Arbitrary value for better visuals
	return float4(material, 1);
}
