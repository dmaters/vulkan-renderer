
#include "bindings/camera.slang"
#include "comp/pbr.slang"
#include "comp/shadow.slang"

[vk::binding(0,2)] uniform Sampler2D _albedo;
[vk::binding(1,2)] uniform Sampler2D _world_position;
[vk::binding(2,2)] uniform Sampler2D _normal;
[vk::binding(3,2)] uniform Sampler2D _roughness_metallic;
[vk::binding(4,2)] uniform Sampler2D _shadow_map;



struct VertexOutput {
    float4 position : SV_Position;
    float2 frag_position : TEXCOORD0;
}



[shader("fragment")]
float4 main(VertexOutput input) : SV_Target{

	float3 cameraPos = (uCamera.view[3].xyz) * float3(1,1,-1);

	float4 fragPos = _world_position.Sample(input.frag_position);

	float3 view_dir = -transpose(uCamera.view)[2].xyz;
	float3 brdf = float3(0);

	float3 albedo = _albedo.Sample(input.frag_position).rgb;
	float3 normal = _normal.Sample(input.frag_position).rgb;
    float2 roughnessMetallic = _roughness_metallic.Sample(input.frag_position).gb;


	float shadowPercentage = Shadow::shadowPercentage(_shadow_map,fragPos.xyz);

	for (int i = 0; i < uLightData.count; i++) {

		float3 light_dir = -transpose(uLightData.lights[i].view)[2].xyz;

		float3 irradiance =  PBR::microfacetBRDF(input.frag_position, view_dir, light_dir,albedo,normal,roughnessMetallic.x,roughnessMetallic.y);
		brdf += (1 - shadowPercentage.x) * irradiance * uLightData.lights[i].color.xyz * uLightData.lights[i].intensity;
	}




	return float4 (brdf + uEnvironment.environment_color, 1);
}
