#include "common/camera.slang"
#include "common/lighting.slang"
#include "common/static_textures.slang"
#include "comp/utils.slang"

namespace Shadow {

static const float CASCADE_BOUNDS[3] = { 125, 700, 4000 };
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
	float distance = -mul(camera.view, float4(worldPos, 1)).z;
	if (distance > CASCADE_BOUNDS[2]) return 1;

	uint cascade = 2;

	if(distance < (CASCADE_BOUNDS[1] + light.paddings[1])) cascade = 1;
	if(distance < (CASCADE_BOUNDS[0] + light.paddings[0])) cascade = 0;

    //We use the padding of the stabilization alignment for the blending
    //This way we access the full cascade resolution before using the next
	float weight = saturate((distance - CASCADE_BOUNDS[cascade]) / light.paddings[cascade]);
    float shadow = getValue(shadowmap, worldPos, normal, cascade, camera, light);

    if(weight > 0 && cascade < 2)
    {
        float nextCascadeShadow = getValue(shadowmap, worldPos, normal, cascade + 1, camera, light);
        shadow = lerp(shadow, nextCascadeShadow, weight);
    }

	return shadow;
}
}  // namespace Shadow
