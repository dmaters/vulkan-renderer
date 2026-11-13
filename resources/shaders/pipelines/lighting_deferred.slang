import definitions.camera;
import definitions.light;
import definitions.pbr;

import static_textures;
import samplers;

import tech.shadowmapping;
import tech.pbr;

import math.spherical_harmonics;


[vk::binding(0)] uniform ConstantBuffer<Camera> _camera;
[vk::binding(1)] uniform ConstantBuffer<Light> _lightData;
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
	float4 fragPos = _world_position.Sample(_samplers[SamplerType::LinearClamp], input.frag_position);

	float3 view_dir = -_camera.direction.xyz;
	float3 material = float3(0);

	float3 albedo = _albedo.Sample(_samplers[SamplerType::LinearClamp], input.frag_position).rgb;
	float3 normal = _normal.Sample(_samplers[SamplerType::LinearClamp], input.frag_position).rgb;
	float2 roughnessMetallic = _roughness_metallic.Sample(_samplers[SamplerType::LinearClamp], input.frag_position).rg;

	float shadowPercentage = cascadeShadowmapSample(_shadow_map, fragPos.xyz, normal.xyz, _camera, _lightData);
	float3 light_dir = _lightData.direction.xyz;
	float3 skylight = _envSH.getValue(-normal);	 // Improve coordinate handling
	float3 brdf = microfacetBRDF(view_dir, light_dir, albedo, normal, roughnessMetallic.x, roughnessMetallic.y);

	material +=
		(1 - shadowPercentage) * brdf * saturate(dot(normal, light_dir)) * _lightData.intensity * _lightData.color;

	material += brdf * skylight * 3.14;  // Arbitrary value for better visuals
	return float4(material, 1);
}
