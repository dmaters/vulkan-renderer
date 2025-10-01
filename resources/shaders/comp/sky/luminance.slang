implementing sky;
__include common;
__include multiscattering;
__include transmittance;


//Cornette Shanks
internal float getMiePhase(float cosTheta)
{
    static const float _g = .8;

    float k = 3.0 / (8.0 * PI) * (1.0 - _g * _g) / (2.0 + _g * _g);
	return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + _g * _g - 2.0 * _g * cosTheta, 1.5);
}
internal float getRayleighPhase(float cosTheta)
{
	float factor = 3.0f / (16.0f * PI);
	return factor * (1.0f + cosTheta * cosTheta);
}



internal float3 getScatteredLuminance(Sampler2D<float4> transmittanceLUT, float height, float3 direction, float3 sunDirection, bool anisotropicPhase = false, bool shadowing = true, bool ground = false) {
    static const uint _sampleCount = 500;
    float mu = dot(direction, float3(0,1,0));
    float nu = dot(direction, sunDirection);


    float dg = distanceToGround(height,mu);
    float da = distanceToAtmosphere(height,mu);
    float distance = dg > 0 ? min(dg,da) : da;
    float dt = distance / float(_sampleCount);


    float t = 0.0f;


    float miePhase, rayleighPhase;
    if(anisotropicPhase)
        miePhase = rayleighPhase = 1.0 / (4.0 * PI);
    else
    {
        miePhase = getMiePhase(nu);	
        rayleighPhase = getRayleighPhase(nu);
    }

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
        
        float3 transmittanceToSun = getTransmittance(transmittanceLUT, pHeight, muS);
        float3 phaseScattering = sample.scatteringRay * rayleighPhase + sample.scatteringMie * miePhase;
        float shadow = shadowing ? 1.0 - saturate(distanceToGround(pHeight, muS)) : 1.0;
        
        
        float3 scattering = shadow * transmittanceToSun * phaseScattering;
    
        
        float3 Sint = (scattering - scattering * sampleTransmittance) / sample.extinction;	
        
        result += throughput * Sint * dt;
        throughput *= sampleTransmittance;


        if(ground && dg > 0 && i == _sampleCount){
            result += transmittanceToSun * throughput * saturate(dot(sunDirection, up)) / PI;
        }
    }


    return result;
}


internal float3 getScatteredLuminanceMultiple(Sampler2D<float4> transmittanceLUT, float height, float3 direction, float3 sunDirection, bool anisotropicPhase = false, bool shadowing = true, bool ground = false, Sampler2D<float4> multiscatteringLUT) {
    static const uint _sampleCount = 500;
    float mu = dot(direction, float3(0,1,0));
    float nu = dot(direction, sunDirection);


    float dg = distanceToGround(height,mu);
    float da = distanceToAtmosphere(height,mu);
    float distance = dg > 0 ? min(dg,da) : da;
    float dt = distance / float(_sampleCount);


    float t = 0.0f;


    float miePhase, rayleighPhase;
    if(anisotropicPhase)
        miePhase = rayleighPhase = 1.0 / (4.0 * PI);
    else
    {
        miePhase = getMiePhase(nu);	
        rayleighPhase = getRayleighPhase(nu);
    }

    float3 throughput = 1.0;
    float3 result = float3(0);
	const float sampleOffset = 0.3f;

    for(uint i = 50; i <= _sampleCount; i++)
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
        
        float3 transmittanceToSun = getTransmittance(transmittanceLUT, pHeight, muS);
        float3 phaseScattering = sample.scatteringRay * rayleighPhase + sample.scatteringMie * miePhase;
        float shadow = shadowing ? 1.0 - saturate(distanceToGround(pHeight, muS)) : 1.0;
        
        
        float3 scattering = shadow * transmittanceToSun * phaseScattering;
        

        float2 uv;
        getMultipleScatteringUV(muS,pHeight,uv);
        scattering += multiscatteringLUT.Sample(uv).rgb * sample.scattering;
        
        float3 Sint = (scattering - scattering * sampleTransmittance) / sample.extinction;	
        
        result += throughput * Sint * dt;
        throughput *= sampleTransmittance;


        if(ground && dg > 0 && i == _sampleCount){
            result += transmittanceToSun * throughput * saturate(dot(sunDirection, up)) / PI;
        }
    }


    return result;
}

internal float3 getScattering(Sampler2D<float4> transmittanceLUT, float height, float3 direction, float3 sunDirection) {
    static const uint _sampleCount = 500;

    float mu = dot(direction, float3(0,1,0));
    float muS = dot(sunDirection, float3(0,1,0));

    float dg = distanceToGround(height,mu);
    float da = distanceToAtmosphere(height,mu);

    float distance = dg > 0 ? min(dg,da) : da;
    float dt = distance / float(_sampleCount);

    float t = 0.0f;

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

		const float3 sampleTransmittance = exp(-sample.extinction * dt);
        
        float3 scattering = sample.scattering;
        float3 Mint = (scattering - scattering * sampleTransmittance) / sample.extinction;	
    
        result += throughput * Mint;
        throughput *= sampleTransmittance;
         
    }
    return result;
}
