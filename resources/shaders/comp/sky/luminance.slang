implementing sky;
__include common;
__include multiscattering;
__include transmittance;


//Cornette Shanks


interface IPhaseProvider { static float rayleighPhase (float nu); static float miePhase(float nu); }

internal struct AnisotropicPhase : IPhaseProvider {
    static float miePhase(float cosTheta)
    {
        static const float _g = .7;

        float k = 3.0 / (8.0 * PI) * (1.0 - _g * _g) / (2.0 + _g * _g);
        return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + _g * _g - 2.0 * _g * cosTheta, 1.5);
    }
    static float rayleighPhase(float cosTheta)
    {
        float factor = 3.0f / (16.0f * PI);
        return factor * (1.0f + cosTheta * cosTheta);
    }

};

internal struct IsotropicPhase : IPhaseProvider {

    static float miePhase(float cosTheta)
    {
        return 1.0 / (4.0 * PI);
    }
    static float rayleighPhase(float cosTheta)
    {
        return 1.0 / (4.0 * PI);
    }
}

internal struct NeutralPhase : IPhaseProvider {
    static float miePhase(float cosTheta)
    {
        return 1.0;
    }
    static float rayleighPhase(float cosTheta)
    {
        return 1.0;
    }
};


interface IMultiscatteringProvider { float3 getMultiscattering(float height, float muS); }
struct  DummyMultiscatteringProvider : IMultiscatteringProvider {
    float3 getMultiscattering(float height, float muS) {return float3(0);}
}
struct LUTMultiscatteringProvider : IMultiscatteringProvider {
    Sampler2D<float4> multiscatteringLUT;
    float3 getMultiscattering(float height, float muS) {
        float2 uv;
        getMultipleScatteringUV(muS,height,uv);
        return multiscatteringLUT.Sample(uv).rgb;
    }
}

struct ScatteredLuminanceRes{
    float3 luminance;
    float3 scattering;
};
internal float3 getScatteredLuminance<
                Phase : IPhaseProvider,
                Multiscattering : IMultiscatteringProvider,
                let Shadow: bool,
                let Ground : bool
            >(
                Sampler2D<float4> transmittanceLUT,
                float height,
                float3 direction,
                float3 sunDirection,
                float3 sunIlluminance = 1,
                Multiscattering multiscatteringProvider = {}
    ) {
    static const uint _sampleCount = 20;
    float mu = dot(direction, float3(0,1,0));
    float nu = dot(direction, sunDirection);


    float dg = distanceToGround(height,mu);
    float da = distanceToAtmosphere(height,mu);
    float distance = dg > 0 ? min(dg,da) : da;
    float dt = distance / float(_sampleCount);


    float t = 0.0f;

    float miePhase = Phase::miePhase(nu);
    float rayleighPhase = Phase::rayleighPhase(nu);

    float3 throughput = 1.0;
    float3 result = float3(0);
	const float sampleOffset = 0.3f;

    for(uint i = 0; i < _sampleCount; i++)
    {

        float NewT = distance * (i + sampleOffset) / _sampleCount;
        dt = NewT - t;
        t = NewT;
        float3 p = float3(0, height, 0) + direction * t;
        float pHeight = length(p);
	    float3 up = p / pHeight;
        float muS = dot(sunDirection, up);

        MediumSample sample = MediumSample::Get(pHeight);


		const float3 T = exp(-sample.extinction * dt);
        
        float3 transmittanceToSun = getTransmittance(transmittanceLUT, pHeight, muS) ;
        float3 phaseScattering = miePhase * sample.scatteringMie + rayleighPhase * sample.scatteringRay;
        float3 shadow = Shadow ? 1.0 - saturate(distanceToGround(pHeight, muS)) : 1.0 ;
        float3 MS = multiscatteringProvider.getMultiscattering(pHeight,muS);
        float3 S = sunIlluminance *  transmittanceToSun * shadow * phaseScattering + MS * sample.scattering;
        float3 Sint = (S - S * T) / sample.extinction;


        result += throughput * Sint;


        if(Ground && dg > 0 && i == _sampleCount - 1){
            result += sunIlluminance * transmittanceToSun * throughput * saturate(muS) * (sample.scattering / max(0.001, sample.extinction)) / PI;
        }
        throughput *= T;
      
    }

    return result;
}