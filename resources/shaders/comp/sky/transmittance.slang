implementing sky;
__include atmosphere;

internal void transmittanceParams(float2 uv, out float height, out float mu) {
	float x_mu = uv.x;
	float x_r = uv.y;

	float H = sqrt(
		Atmosphere.TopRadius * Atmosphere.TopRadius -
		Atmosphere.BottomRadius * Atmosphere.BottomRadius
	);
	float rho = H * x_r;
	height =
		sqrt(rho * rho + Atmosphere.BottomRadius * Atmosphere.BottomRadius);

	float d_min = Atmosphere.TopRadius - height;
	float d_max = rho + H;
	float d = d_min + x_mu * (d_max - d_min);
	mu = d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * height * d);
	mu = clamp(mu, -1.0, 1.0);
}

internal void transmittanceUV(float height, float mu, out float2 uv) {
	float H = sqrt(
		Atmosphere.TopRadius * Atmosphere.TopRadius -
		Atmosphere.BottomRadius * Atmosphere.BottomRadius
	);
	float rho = sqrt(
		height * height - Atmosphere.BottomRadius * Atmosphere.BottomRadius
	);

	float d = distanceToAtmosphere(height, mu);

	float d_min = Atmosphere.TopRadius - height;
	float d_max = rho + H;
	float x_mu = (d - d_min) / (d_max - d_min);
	float x_r = rho / H;

	uv = float2(x_mu, x_r);
}

float3 getTransmittance(
	uniform Texture2D transmittanceLUT, float height, float mus
) {
	float2 uv;
	transmittanceUV(height, mus, uv);
	return transmittanceLUT
	    .SampleLevel(_samplers[SamplerType::LinearClamp], uv, 0)
	    .rgb;
}

internal float3 computeTrasmittance(float height, float mu) {
	static const uint _sampleCount = 64;

	float dx = distanceToAtmosphere(height, mu) / _sampleCount;

	float3 opticalLength = float3(0);

	for (uint i = 0; i < _sampleCount; i++) {
		float d = i * dx;
		float sampleHeight =
			sqrt(d * d + 2.0 * height * mu * d + height * height);

		MediumSample sample = MediumSample::Get(sampleHeight);
		opticalLength += sample.extinction;
	}

	return exp(-opticalLength * dx);
}
