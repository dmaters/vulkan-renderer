implementing sky;
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

//TODO: Few problems on defining this in a shared module, keep it duplicate until a fix is found
public float3 fibonacciSampling<let Semisphere : bool>(uint i, uint N)
{
	
	static const float gR = PI * (sqrt(5) + 1.0); 
	float theta = i * gR;
    float phi = 0;
    if(Semisphere)
	    phi = 1.0 - 1.0 * i / (N - 1);	
    else
	    phi = 1.0 - 2.0 * i / (N - 1);	

    phi = acos(clamp(phi,-1.0, 1.0));

	float3 dir;

	dir.x = cos(theta) * sin(phi);
	dir.y = cos(phi); 
	dir.z = sin(theta) * sin(phi);


	return normalize(dir);
}


internal float3 computeMultipleScattering(Sampler2D transmittanceLUT, float height, float3 sunDirection, uint3 threadID, RWStructuredBuffer<float> cache ){

	static const uint _samples = 64;

	
	const float angle = 4.0 * PI;
	const float phase = 1.0 / angle;

	

	float3 dir = fibonacciSampling<false>(threadID.x,_samples);
	ScatteredLuminanceResult res = getScatteredLuminance<IsotropicPhase,DummyMultiscatteringProvider,false,true>(transmittanceLUT, height, dir, sunDirection) ;
	cache[threadID.x * 3 + 0] = res.luminance.r;
	cache[threadID.x * 3 + 1] = res.luminance.g;
	cache[threadID.x * 3 + 2] = res.luminance.b;


	cache[threadID.x * 3 + _samples * 3 + 0] = res.luminanceFactor.r;
	cache[threadID.x * 3 + _samples * 3 + 1] = res.luminanceFactor.g;
	cache[threadID.x * 3 + _samples * 3 + 2] = res.luminanceFactor.b;
	
	AllMemoryBarrierWithGroupSync();


	if(threadID.x > 0)	return 0;

	float3 luminance2Ord = float3(0);
	float3 factor = float3(0);
	for(int i = 0; i < _samples; i++){
		luminance2Ord += float3(
			cache[threadID.x * 3 + 0],
			cache[threadID.x * 3 + 1],
			cache[threadID.x * 3 + 2]
		);
		factor += float3(
			cache[threadID.x * 3 + _samples * 3 + 0],
			cache[threadID.x * 3 + _samples * 3 + 1],
			cache[threadID.x * 3 + _samples * 3 + 2]
		);
	}

	float3 l = luminance2Ord / _samples;
	float3 f = 1.0 / (1.0 - factor / _samples);
	
	return f * l ;
}