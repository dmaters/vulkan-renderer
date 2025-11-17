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


internal float3 computeMultipleScattering(Sampler2D<vector<float,4>> transmittanceLUT, float height, float3 sunDirection){

	static const uint _samples = 64;

	float3 luminance2Ord = float3(0);
	float3 factor = float3(0);
	const float angle = 4.0 * PI;
	const float phase = 1.0 / angle;


	for(int i = 0; i < _samples; i++) {

		float3 dir = fibonacciSampling<false>(i,_samples);
		ScatteredLuminanceResult res = getScatteredLuminance<IsotropicPhase,DummyMultiscatteringProvider,false,true>(transmittanceLUT, height, dir, sunDirection) ;
		luminance2Ord += res.luminance;
		factor += res.luminanceFactor;
	
	}

	float3 l = luminance2Ord / _samples;
	float3 f = 1.0 / (1.0 - factor / _samples);
	
	return f * l ;
}