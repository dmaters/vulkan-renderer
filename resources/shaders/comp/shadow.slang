#include "common/camera.slang"
#include "common/lighting.slang"
#include "common/static_textures.slang"
#include "comp/utils.slang"

namespace Shadow {

static const float CASCADE_BOUNDS[4] = { 0.1, 125, 700, 2500 };
static const float CASCADE_PADDING = 30;
float4x4 shadowProjectionMatrix(uint cascade, Camera camera, LightData light) {
	float right = -1e9, left = 1e9, top = -1e9, bottom = 1e9, far = 1e9;
	float3[8] frustumPoints;

	for (int i = 0; i < 4; i++) {
		frustumPoints[i] = camera.frustumPoints[cascade][i].xyz;
		frustumPoints[i + 4] = camera.frustumPoints[cascade + 1][i].xyz;
	}

	for (int i = 0; i < 8; i++) {
		float4 point = mul(light.view, float4(frustumPoints[i], 1));

		right = max(right, point.x);
		left = min(left, point.x);
		top = max(top, point.y);
		bottom = min(bottom, point.y);
		far = min(far, point.z);
	}

	float worldUnitsPerTexelx = (right - left) / 1024.0;
	float worldUnitsPerTexely = (top - bottom) / 1024.0;

	left -= CASCADE_PADDING;
	bottom -= CASCADE_PADDING;
	right += CASCADE_PADDING;
	top += CASCADE_PADDING;

	left = floor(left / worldUnitsPerTexelx) * worldUnitsPerTexelx;
	bottom = floor(bottom / worldUnitsPerTexely) * worldUnitsPerTexely;
	right = floor(right / worldUnitsPerTexelx) * worldUnitsPerTexelx;
	top = floor(top / worldUnitsPerTexely) * worldUnitsPerTexely;

	return orthoProjectionMatrix(left, right, bottom, top, 0, -far);
}

float getValue(
	Texture2D shadowMap,
	float3 worldPos,
	uint cascade,
	Camera camera,
	LightData light
) {
	float4 lightSpacePos =
		mul(mul(shadowProjectionMatrix(cascade, camera, light), light.view),
	        float4(worldPos, 1));

	lightSpacePos.y = -lightSpacePos.y;

	float2 uv = lightSpacePos.xy * 0.5 + 0.5;

	uv.x /= 3.0;
	uv.x += (1.0 / 3.0) * cascade;

	return (lightSpacePos.z - 0.005) <
	               shadowMap.Sample(_samplers[SamplerType::LinearClamp], uv).r
	           ? 0
	           : 1;
}
float shadowPercentage(
	Texture2D shadowmap, float3 worldPos, Camera camera, LightData light
) {
	float distance = abs(mul(camera.view, float4(worldPos, 1)).z);
	if (distance > CASCADE_BOUNDS[3]) return 1;

	uint cascade = distance < CASCADE_BOUNDS[2] ? 1 : 2;
	cascade = distance < (CASCADE_BOUNDS[1]) ? 0 : cascade;

	float weight = saturate(
		(distance - (CASCADE_BOUNDS[cascade + 1] - CASCADE_PADDING)) /
		(2 * CASCADE_PADDING)
	);

	weight = weight * weight * (3 - 2 * weight);
	float depth = lerp(
		getValue(shadowmap, worldPos, cascade, camera, light),
		getValue(shadowmap, worldPos, cascade + 1, camera, light),
		weight
	);

	return depth;
}
}  // namespace Shadow
