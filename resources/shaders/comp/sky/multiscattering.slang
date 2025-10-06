implementing sky;
__include common;
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


float3 fibonacciSampling(uint i, uint N)
{
	
	float gR = (1.0 + sqrt(5)) / 2.0; 
	float theta = 2.0f * PI * i / gR;
	float phi = acos(1.0 - 2.0 * i / N);	

	float3 dir;

	dir.x = cos(theta) * sin(phi);
	dir.y = sin(theta) * sin(phi);
	dir.z = cos(phi);


	return normalize(dir);
}


internal float3 computeMultipleScattering(Sampler2D<vector<float,4>> transmittanceLUT, float height, float3 sunDirection){

	static const uint _samples = 64;

	float3 luminance2Ord = float3(0);
	float3 factor = float3(0);
	const float angle = 4.0 * PI;
	const float phase = 1.0 / angle;


	for(int i = 0; i < _samples; i++) {

		float3 dir = fibonacciSampling(i,_samples);
		ScatteredLuminanceResult res = getScatteredLuminance<IsotropicPhase,DummyMultiscatteringProvider,false,true>(transmittanceLUT, height, dir, -sunDirection) ;
		luminance2Ord += res.luminance;
		factor += res.luminanceFactor;
	
	}

	float3 l = luminance2Ord / _samples;
	float3 f = 1.0 / (1.0 - factor / _samples);
	
	return f * l ;
}