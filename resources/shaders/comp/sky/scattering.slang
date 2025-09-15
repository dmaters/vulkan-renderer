implementing sky;
__include common;
__include transmittance;


//Cornette Shanks
internal float miePhase(float cosTheta)
{
    static const float _g = 0.8;

    float k = 3.0 / (8.0 * PI) * (1.0 - _g * _g) / (2.0 + _g * _g);
	return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + _g * _g - 2.0 * _g * -cosTheta, 1.5);
}
internal float rayleighPhase(float cosTheta)
{
	float factor = 3.0f / (16.0f * PI);
	return factor * (1.0f + cosTheta * cosTheta);
}


internal float4 computeScattering(float height, float mu, float muS){


    uint _samples = 50;


    float dx = distanceToAtmosphere(height, mu);

    float4 scattering = float4(0);
    
    for(int i = 0; i <_samples ; i++){



        float dxi = height + dx * i;
        
        float3 tInitialPos = getTransmittance(dxi, mu);
        float3 tLight = getTransmittance(dxi, muS);

        float3 scatteringRayleigh = -exp(-tInitialPos* Atmosphere.RayleighScattering - tLight * Atmosphere.RayleighScattering);
    }


}






internal float3 getScattering(float height, float mu, float muS, float nu){

    Sampler3D<float4> _scatteringLUT;

	float miePhaseValue = miePhase(-muS);
	float rayleighPhaseValue = rayleighPhase(muS);
    float3 phaseScattering; 

    
    


}
