#include "bindings/lighting.slang"
#include "bindings/view_proj.slang"
#include "comp/lighting.slang"


[vk::binding(0,2)] uniform Sampler2D _albedo;
[vk::binding(1,2)] uniform Sampler2D _world_position;
[vk::binding(2,2)] uniform Sampler2D _normal;
[vk::binding(3,2)] uniform Sampler2D _roughness_metallic;

struct VertexOutput {
    float4 position : SV_Position;
    float2 frag_position : TEXCOORD0;
}



[shader("fragment")]
float4 main(VertexOutput input) : SV_Target{
	float3 cameraPos = (transpose(VIEW))[3].xyz;

	float4 fragPos = _world_position.Sample(input.frag_position);

	float3 view_dir = normalize(cameraPos - fragPos.xyz);

	float3 brdf = float3(0);


    float3 albedo = _albedo.Sample(input.frag_position).rgb;
    float3 normal = _normal.Sample(input.frag_position).rgb;
    float2 roughnessMetallic = _roughness_metallic.Sample(input.frag_position).gb;

	for (int i = 0; i < LIGHT_COUNT; i++) {

		float4x4 light_mat = transpose(LIGHT_DATA[i].trasformation);
		float3 light_dir;
		light_dir = normalize(light_mat[3].xyz - fragPos.xyz);
		
	
		float3 irradiance = microfacetBRDF(input.frag_position, view_dir, light_dir,albedo,normal,roughnessMetallic.x,roughnessMetallic.y);
		brdf +=  irradiance * LIGHT_DATA[i].color.xyz * LIGHT_DATA[i].intensity;
	}

	return float4 (brdf, 1);
}