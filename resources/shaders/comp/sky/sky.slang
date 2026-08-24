module sky;
#include "common/static_textures.slang"
// The following implementation follows the paper "A Scalable and Production
// Ready, Sky and Atmosphere Rendering Technique" by Sébastien Hillaire The code
// partly follows the example provided For a more accurate implementation please
// go to the source

import comp.common.spherical_harmonics;
import comp.common;
__include luminance;

public
void writeTransmittanceLUT(WTexture2D<float4> transmittanceLUT, uint2 coord) {
	float2 res;
	transmittanceLUT.GetDimensions(res.x, res.y);

	float height, mu;
	transmittanceParams(float2(coord) / res.xy, height, mu);

	transmittanceLUT.Store(coord, float4(computeTrasmittance(height, mu), 1));
}

public
float3 readSkyViewLUT(Texture2D skyviewLUT, float3 direction) {
	float lat = asin(direction.y);
	float lon = atan2(direction.x, direction.z);
	float2 uv;

	uv.x = lon / PI;
	uv.y = (1.0 + sign(lat) * sqrt(2.0 * abs(lat) / PI)) / 2;
	uv.x = abs(uv.x);
	return skyviewLUT.SampleLevel(_samplers[SamplerType::LinearClamp], uv, 0).rgb;
}

public
void writeSkyViewLUT(
	WTexture2D<float4> skyviewLUT,
	Texture2D transmittanceLUT,
	Texture2D multitransferLUT,
	uint2 coord,
	float3 sunDirection,
	float sunIntensity
) {
	float2 res;
	skyviewLUT.GetDimensions(res.x, res.y);
	float2 uv = (float2(coord.xy) / res.xy);
	float lon = (uv.x * 2.0 - 1) * PI / 2;
	float t = uv.y * 2 - 1;
	float lat = sign(t) * t * t * PI / 2;

	float3 direction = float3(cos(lon) * cos(lat), sin(lat), cos(lat) * sin(lon));

	float height = Atmosphere.BottomRadius + 0.1;
	float3 luminance = getScatteredLuminance<AnisotropicPhase, LUTMultiscatteringProvider, true, false>(
						   transmittanceLUT, height, direction, sunDirection, 1, { multitransferLUT }
	)
						   .luminance;
	skyviewLUT.Store(coord, float4(luminance, 1));
}

public
void writeMultiscatteringLUT(
	WTexture2D<float4> multiscatteringLUT,
	Texture2D transmittanceLUT,
	uint2 coord,
	uint thread,
	RWStructuredBuffer<float> cache
) {
	float2 res;
	multiscatteringLUT.GetDimensions(res.x, res.y);
	float2 uv = (coord.xy + float2(0.5)) / res;

	float muS;
	float height;
	getMultipleScatteringParams(uv, muS, height);

	float3 sunDir = normalize(float3(0, muS, sqrt(1.0 - muS * muS)));

	uint _samples = 64;

	uint cacheOffset = (coord.y * uint(res.x) + coord.x) * _samples * 3;

	float3 multiscattering =
		computeMultipleScattering(transmittanceLUT, height, sunDir, _samples, thread, cacheOffset, cache);
	if (thread == 0) multiscatteringLUT.Store(coord, float4(multiscattering, 1));
}
