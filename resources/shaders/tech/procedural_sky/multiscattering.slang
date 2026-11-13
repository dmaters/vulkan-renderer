implementing procedural_sky;
import math.sample;

__include atmosphere;
__include luminance;

internal void getMultipleScatteringUV(float muS, float height, out float2 uv) {
	float altitude = height - Atmosphere.BottomRadius;
	uv.x = 0.5 + 0.5 * muS;
	uv.y = saturate(altitude / (Atmosphere.TopRadius - Atmosphere.BottomRadius));
}
internal void getMultipleScatteringParams(float2 uv, out float muS, out float height) {
	muS = uv.x * 2.0 - 1.0;
	height = (uv.y * (Atmosphere.TopRadius - Atmosphere.BottomRadius)) + Atmosphere.BottomRadius;
}


internal float3 computeMultipleScattering(
	Texture2D transmittanceLUT,
	float height,
	float3 sunDirection,
	uint samples,
	uint sampleIndex,
	uint cacheOffset,
	RWStructuredBuffer<float> cache
) {
	const float angle = 4.0 * PI;
	const float phase = 1.0 / angle;

	float3 dir = -fibonacciSamplingSphere(sampleIndex, samples);
	ScatteredLuminanceResult res = getScatteredLuminance<IsotropicPhase, DummyMultiscatteringProvider, false, true>(
		transmittanceLUT, height, dir, sunDirection
	);

	float3 sampleLuminance = (res.luminance / samples) / (1.0 - res.luminanceFactor / samples);
	cache[cacheOffset + sampleIndex * 3 + 0] = sampleLuminance.r;
	cache[cacheOffset + sampleIndex * 3 + 1] = sampleLuminance.g;
	cache[cacheOffset + sampleIndex * 3 + 2] = sampleLuminance.b;

	AllMemoryBarrierWithGroupSync();

	if (sampleIndex > 0) return 0;

	float3 luminance = float3(0);
	for (int i = 0; i < samples; i++) {
		luminance +=
			float3(cache[cacheOffset + i * 3 + 0], cache[cacheOffset + i * 3 + 1], cache[cacheOffset + i * 3 + 2]);
	}

	return luminance;
}
