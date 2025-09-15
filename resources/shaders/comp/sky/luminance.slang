implementing sky;
__include common;
__include multiscattering;



internal float heightTo01(float height) {
    return sqrt((pow(height,2) - pow(Atmosphere.BottomRadius,2)) / (pow(Atmosphere.TopRadius,2) / pow(Atmosphere.BottomRadius,2)));
}
internal float viewAziTo01(float angle) {
    return (1 + cos(angle))/2;
}
internal float sunAziTo01(float angle){
    return (1- exp(-2.8 * cos(angle) - 0.8)) /(1 - exp(- 3.6));
}






internal float3 integrateScatteredLuminance(float height, float3 direction, float3 sunDirection) {


    static const uint _sampleCount = 500;
    static const float _sunLuminance = 1;
    float viewSunAngle = acos(dot(sunDirection, sunDirection));
    float viewAziAngle = acos(dot(direction, float3(0,1,0)));
    float sunAziAngle = acos(dot(direction, float3(0,1,0)));

	float cosWorldSun= dot(direction, sunDirection);

    float dx = distanceToAtmosphere(height,viewSunAngle) / _sampleCount;
    
    Sampler2D<float> t_transmittanceLUT;

    for(uint i = 0; i < _sampleCount; i++)
    {
        
        float2 transmittanceUV = float2(viewAziTo01(viewAziAngle),sunAziTo01(sunAziAngle));
        float transmittance = t_transmittanceLUT.Sample(transmittanceUV).r;

        MediumSample medium = MediumSample::Get(height);
        float3 multiscatteredLuminance = getMultipleScattering(medium.scattering,medium.extinction,height - Atmosphere.BottomRadius,cos(viewAziAngle));
        float3 S = _sunLuminance * (transmittance * phaseScattering + multiscatteredLuminance * medium.scattering);

    }
}


