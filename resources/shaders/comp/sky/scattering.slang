implementing sky;
__include common;
__include transmittance;





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

}
