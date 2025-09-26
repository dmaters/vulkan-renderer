implementing sky;
__include common;
__include multiscattering;
__include transmittance;


internal float heightTo01(float height) {
    return sqrt((pow(height,2) - pow(Atmosphere.BottomRadius,2)) / (pow(Atmosphere.TopRadius,2) / pow(Atmosphere.BottomRadius,2)));
}
internal float viewAziTo01(float angle) {
    return (1 + cos(angle))/2;
}
internal float sunAziTo01(float angle){
    return (1- exp(-2.8 * cos(angle) - 0.8)) /(1 - exp(- 3.6));
}




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

internal float3 integrateScatteredLuminance(Sampler2D<float4> transmittanceLUT, float height, float3 direction, float3 sunDirection) {


    static const uint _sampleCount = 500;

    float mu = dot(direction, float3(0,1,0));
    float muS = dot(sunDirection, float3(0,1,0));
    float nu = dot(direction, sunDirection);

    float dg = distanceToGround(height,mu);
    float da = distanceToAtmosphere(height,mu);

    float distance = dg > 0 ? min(dg,da) : da;
    float dt = distance / _sampleCount;


    float t = 0.0f;

    float miePhase = miePhase(-nu);	
    float rayleighPhase = rayleighPhase(nu);


    float3 throughput = 1.0;
    float3 result = float3(0);
	const float sampleOffset = 0.3f;

    for(uint i = 0; i <= _sampleCount; i++)
    {

        float NewT = distance * (i + sampleOffset) / _sampleCount;
        dt = NewT - t;
        t = NewT;
        float3 p = float3(0,height,0 ) + direction * t;
        float pHeight = length(p);
	    float3 up = p / pHeight;
        float muS = dot(sunDirection, up);

        MediumSample sample = MediumSample::Get(pHeight);

        const float3 sampleOpticalDepth = sample.extinction * dt;
		const float3 sampleTransmittance = exp(-sampleOpticalDepth);
        
        float3 transmittanceToSun = getTransmittance(transmittanceLUT,pHeight, muS);
        float3 phaseScattering = sample.scatteringRay * rayleighPhase + sample.scatteringMie * miePhase;
        float shadow = 1 - saturate(distanceToGround(pHeight,muS));
        float3 scattering = 20 * shadow * transmittanceToSun * phaseScattering;
        float3 Sint = (scattering - scattering * sampleTransmittance) / sample.extinction;	
        
        
        result += throughput * Sint;
        throughput *= sampleTransmittance;
         
    }
    return result;
}


