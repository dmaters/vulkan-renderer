
#include "common/lighting.slang"
#include "common/static_textures.slang"
#include "comp/sky/sky.slang"

[vk_binding(0)] uniform ConstantBuffer<LightData> _lightData;
[vk_binding(1)] Texture2D _transmittanceLUT;
[vk_binding(2)] Texture2D _multiscatteringLUT;
[vk_binding(3)] WTexture2D<float4> _skyviewLUT;

void main(uint3 dispatch : SV_GroupID) {
	float3 sunDirection = -transpose(_lightData.view)[2].xyz;
	sunDirection.y = -sunDirection.y;
	writeSkyViewLUT(
		_skyviewLUT,
		_transmittanceLUT,
		_multiscatteringLUT,
		dispatch.xy,
		sunDirection,
		_lightData.intensity
	);
}
