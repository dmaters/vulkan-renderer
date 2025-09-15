
implementing sky;
__include common;


Texture2D<float4> _transmittanceLUT;


float2 tramittanceUV(float height, float mu)
{
	float H = sqrt(max(0.0f, Atmosphere.TopRadius * Atmosphere.TopRadius - Atmosphere.BottomRadius * Atmosphere.BottomRadius));
	float rho = sqrt(max(0.0f, height * height - Atmosphere.BottomRadius * Atmosphere.BottomRadius));

	float discriminant = height * height * (mu * mu - 1.0) + Atmosphere.TopRadius * Atmosphere.TopRadius;
	float d = max(0.0, (-height * mu + sqrt(discriminant))); // Distance to atmosphere boundary

	float d_min = Atmosphere.TopRadius - height;
	float d_max = rho + H;
	float x_mu = (d - d_min) / (d_max - d_min);
	float x_r = rho / H;

	return float2(x_mu, x_r);
}



internal float opticalLength(float density, float height, float mu){

    static const uint _sampleCount = 500;
    float dx = distanceToAtmosphere(height,mu) / _sampleCount;
    float mu = cos(mu);
    float result;

    for(uint i = 0; i < _sampleCount; i++) {
        float d = i * dx;
        float r = sqrt(d * d + 2.0 * height * mu * d + height * height);
        float y = density;
        float weight =  i == 0 || i == _sampleCount ? 0.5 : 1.0;
    
        result += y * weight * dx;
    }
    return result;
}

internal float3 computeTrasmittance(float height, float mu)
{

   	const float densityMie = exp(Atmosphere.MieDensityExpScale * height);
	const float densityRay = exp(Atmosphere.RayleighDensityExpScale * height);

    return exp(-(
        Atmosphere.RayleighScattering * opticalLength(densityRay, height, mu) +
        Atmosphere.MieExtinction * opticalLength(densityMie, height, mu) 
       //ozone
    ));
}


internal void computeTransmittanceLUT(){

    WTexture2D<float3> _transmittanceLUTWrite;

    uint width, height, depth;
    _transmittanceLUT.GetDimensions(0,width,height,depth);


    float2 dP = float2(
        2 / width,
        (Atmosphere.TopRadius - Atmosphere.BottomRadius) / height
    );

    float2 bP = float2(
        -1,
        Atmosphere.BottomRadius
    );

    for(uint x = 0; x< width; x++)
    {
        for(uint y = 0; y < height; y++)
        {
            float2 params = bP + dP * float2(x,y); 
            

            _transmittanceLUTWrite.Store(uint2(x,y),computeTrasmittance(params.x,params.y));
        
        }
    }

}


internal float3 getTransmittance(float height, float mu) {



    

    

}