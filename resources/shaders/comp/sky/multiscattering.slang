implementing sky;
__include common;
__include luminance;

internal void getMultipleScatteringUV(float muS, float height, out float2 uv) {

	float altitude = height - Atmosphere.BottomRadius;
	uv.x = 0.5 + 0.5 * muS;
	uv.y = saturate(altitude / (Atmosphere.TopRadius - Atmosphere.BottomRadius));
}
internal void getMultipleScatteringParams(float2 uv, out float muS, out float height) {
	muS = (uv.x - 0.5) * 2.0;
	height = (uv.y * (Atmosphere.TopRadius - Atmosphere.BottomRadius)) + Atmosphere.BottomRadius;
}


float3 fibonacciSampling(uint i, uint N)
{
	static const float _p = (1.0 + sqrt(5.0)) / 2.0;

	float y = 1.0 - (2.0 * i + 1.0) / N;
	float phi = 2.0 * PI * i / _p;


	float3 dir;

	float r = sqrt(1 - y * y);
	dir.x = r * cos(phi);
	dir.y = y;
	dir.z = r * sin(phi);

	return dir;
}


internal float3 computeMultipleScattering(Sampler2D<vector<float,4>> transmittanceLUT, float height, float3 sunDirection){

	static const uint _samples = 64;

	float3 luminance2Ord = float3(0);
	float3 factor = float3(0);
	const float angle = 4.0 * PI;
	const float phase = 1.0 / angle;


	for(int i = 0; i < _samples; i++) {
		float3 dir = fibonacciSampling(i,_samples);
		luminance2Ord += getScatteredLuminance(transmittanceLUT, height, dir, -sunDirection, true, true, true) * angle;
		factor += getScattering(transmittanceLUT, height, dir, -sunDirection) * angle;

	}
	float3 l = luminance2Ord * phase / _samples;
	float3 f = 1.0 / (1.0 - factor * phase / _samples);
	
	return l * f;
}