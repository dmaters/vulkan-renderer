implementing sky;
__include atmosphere;
__include luminance;

internal void getMultipleScatteringUV(float muS, float height, out float2 uv) {
	float altitude = height - Atmosphere.BottomRadius;
	uv.x = 0.5 + 0.5 * muS;
	uv.y =
		saturate(altitude / (Atmosphere.TopRadius - Atmosphere.BottomRadius));
}
internal void getMultipleScatteringParams(
	float2 uv, out float muS, out float height
) {
	muS = uv.x * 2.0 - 1.0;
	height = (uv.y * (Atmosphere.TopRadius - Atmosphere.BottomRadius)) +
	         Atmosphere.BottomRadius;
}

// TODO: Few problems on defining this in a shared module, keep it duplicate
// until a fix is found
public
float3 fibonacciSampling<let Semisphere : bool>(uint i, uint N) {
	static const float gR = PI * (sqrt(5) + 1.0);
	float theta = i * gR;
	float phi = 0;
	if (Semisphere)
		phi = 1.0 - 1.0 * i / (N - 1);
	else
		phi = 1.0 - 2.0 * i / (N - 1);

	phi = acos(clamp(phi, -1.0, 1.0));

	float3 dir;

	dir.x = cos(theta) * sin(phi);
	dir.y = cos(phi);
	dir.z = sin(theta) * sin(phi);

	return normalize(dir);
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

	float3 dir = -fibonacciSampling<false>(sampleIndex, samples);
	ScatteredLuminanceResult res = getScatteredLuminance<
		IsotropicPhase,
		DummyMultiscatteringProvider,
		false,
		true>(transmittanceLUT, height, dir, sunDirection);

	float3 sampleLuminance =
		(res.luminance / samples) / (1.0 - res.luminanceFactor / samples);
	cache[cacheOffset + sampleIndex * 3 + 0] = sampleLuminance.r;
	cache[cacheOffset + sampleIndex * 3 + 1] = sampleLuminance.g;
	cache[cacheOffset + sampleIndex * 3 + 2] = sampleLuminance.b;

	AllMemoryBarrierWithGroupSync();

	if (sampleIndex > 0) return 0;

	float3 luminance = float3(0);
	for (int i = 0; i < samples; i++) {
		luminance += float3(
			cache[cacheOffset + i * 3 + 0],
			cache[cacheOffset + i * 3 + 1],
			cache[cacheOffset + i * 3 + 2]
		);
	}

	return luminance;
}
