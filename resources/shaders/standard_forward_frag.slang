#include "standard_forward.slang"
#include "lighting.slang"


struct LightData {
	uint count;
	Light[256] lights;
};

[[vk::binding(1, 0)]] ConstantBuffer<LightData> lights;

[[vk::binding(0 ,2)]] Sampler2D albedo;

[shader("fragment")]
float4 main(VertexOutput vertexOutput) : SV_Target {
	float4 outColor = float4(0, 0, 0, 1);

	for (int i = 0; i < lights.count; i++) {
		Light light = lights.lights[i];

		outColor += float4(
			albedo.Sample(vertexOutput.fragTexCoord).rgb * light.color * dot(vertexOutput.fragNormal,  vertexOutput.fragPos.xyz - light.trasformation[3].xyz) *
				light.intensity ,
			1.0
		);
	}

	return outColor;

	
}