
#include "bindings/camera.slang"
#include "bindings/lighting.slang"
#include "comp/pbr.slang"
#include "comp/shadow.slang"

import comp.common.spherical_harmonics;

[vk::binding(0,2)] uniform Sampler2D _albedo;
[vk::binding(1,2)] uniform Sampler2D _world_position;
[vk::binding(2,2)] uniform Sampler2D _normal;
[vk::binding(3,2)] uniform Sampler2D _roughness_metallic;
[vk::binding(4,2)] uniform Sampler2D _shadow_map;
[vk::binding(5,2)] uniform ConstantBuffer<SphericalHarmonic> _envSH;



struct VertexOutput {
    float4 position : SV_Position;
    float2 frag_position : TEXCOORD0;
}



[shader("fragment")]
float4 main(VertexOutput input) : SV_Target{
	float3 cameraPos = (uCamera.view[3].xyz) * float3(1,1,-1);
	input.frag_position.y = 1 - input.frag_position.y;
	float4 fragPos = _world_position.Sample(input.frag_position);

	float3 view_dir = -transpose(uCamera.view)[2].xyz;
	float3 material = float3(0);

	float3 albedo = _albedo.Sample(input.frag_position).rgb;
	float3 normal = _normal.Sample(input.frag_position).rgb;
    float2 roughnessMetallic = _roughness_metallic.Sample(input.frag_position).gb;

	
	float shadowPercentage =  Shadow::shadowPercentage(_shadow_map,fragPos.xyz);
	float3 light_dir = -transpose(uLightData.view)[2].xyz;
	float3 skylight = _envSH.getValue(normal * float3(1,-1,1)); 
	float3 brdf = PBR::microfacetBRDF(input.frag_position, view_dir, light_dir,albedo,normal,roughnessMetallic.x,roughnessMetallic.y);
	material += (1 - shadowPercentage.x) * brdf * uLightData.intensity * uLightData.color;
	material += skylight * 2.7; //Arbitrary value for better visuals
	return float4 (material + uEnvironment.environment_color, 1);
}
