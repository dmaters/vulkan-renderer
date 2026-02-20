#include "common/camera.slang"
#include "common/lighting.slang"
#include "common/static_textures.slang"
#include "comp/utils.slang"

namespace Shadow {

static const float CASCADE_BOUNDS[3] = { 125, 700, 2500 };
float getValue(
	Texture2D shadowMap,
	float3 worldPos,
	float3 normal,
	uint cascade,
	Camera camera,
	LightData light
) {
	float4 lightSpacePos =
		mul(mul(light.cascadeProjections[cascade], light.view),
	        float4(worldPos, 1));

	lightSpacePos.y = -lightSpacePos.y;

	float2 uv = lightSpacePos.xy * 0.5 + 0.5;

	uv.x /= 3.0;
	uv.x += (1.0 / 3.0) * cascade;

	float bias = 1e-4;
	float costheta = dot(normal, -transpose(light.view)[2].xyz);
	bias *= tan(acos(costheta));
	bias = clamp(bias, 0.001, 0.05);

	return (lightSpacePos.z) <
	               shadowMap.Sample(_samplers[SamplerType::LinearClamp], uv).r +
	                   bias
	           ? 0
	           : 1;
}
float shadowPercentage(
	Texture2D shadowmap,
	float3 worldPos,
	float3 normal,
	Camera camera,
	LightData light
) {
	float distance = abs(mul(camera.view, float4(worldPos, 1)).z);
	if (distance > CASCADE_BOUNDS[2]) return 1;

	uint cascade = distance < CASCADE_BOUNDS[1] ? 1 : 2;
	cascade = distance < (CASCADE_BOUNDS[0]) ? 0 : cascade;

	float blendStart =  CASCADE_BOUNDS[cascade] - light.paddings[cascade + 1];
	float blendEnd = CASCADE_BOUNDS[cascade] + light.paddings[cascade];

	float weight = saturate((distance - blendStart) / (blendEnd - blendStart));
	weight = weight * weight * (3 - 2 * weight);

	float depth = lerp(
		getValue(shadowmap, worldPos, normal, cascade, camera, light),
		getValue(shadowmap, worldPos, normal, cascade + 1, camera, light),
		weight
	);
	return depth;
}
}  // namespace Shadow
