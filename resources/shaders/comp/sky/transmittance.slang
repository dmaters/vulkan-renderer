
implementing sky;
__include common;

internal struct Transmittance {
    float height;
    float mu;
};
internal Transmittance transmittanceP(float2 uv)
{
	Transmittance res;
    float x_mu = uv.x;
	float x_r = uv.y;

	float H = sqrt(Atmosphere.TopRadius * Atmosphere.TopRadius - Atmosphere.BottomRadius * Atmosphere.BottomRadius);
	float rho = H * x_r;
	res.height = sqrt(rho * rho + Atmosphere.BottomRadius * Atmosphere.BottomRadius);

	float d_min = Atmosphere.TopRadius - res.height;
	float d_max = rho + H;
	float d = d_min + x_mu * (d_max - d_min);
	res.mu = d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * res.height * d);
	res.mu  = clamp(res.mu, -1.0, 1.0);

    return res;
}

internal float2 transmittanceUV(float height, float mu)
{
	float H = sqrt( Atmosphere.TopRadius * Atmosphere.TopRadius - Atmosphere.BottomRadius * Atmosphere.BottomRadius);
	float rho = sqrt(height * height - Atmosphere.BottomRadius * Atmosphere.BottomRadius);

	float d = distanceToAtmosphere(height,mu);

	float d_min = Atmosphere.TopRadius - height;
	float d_max = rho + H;
	float x_mu = (d - d_min) / (d_max - d_min);
	float x_r = rho / H;

	return float2(x_mu, x_r);
}



float3 getTransmittance(Sampler2D<float4> transmittanceLUT, float height, float mus) {
  return transmittanceLUT.Sample(transmittanceUV(height, mus)).rgb;
}


internal float opticalLength(float densityScale, float height, float mu){

    static const uint _sampleCount = 500;


    float dx = distanceToAtmosphere(height,mu) / _sampleCount;
    float result = 0;

    for(uint i = 0; i <= _sampleCount; i++) {
        float d = i * dx;
        float r = sqrt(d * d + 2.0 * height * mu * d + height * height);
        float y = exp(densityScale * (r - Atmosphere.BottomRadius));
        y = saturate(y);
        float weight =  i == 0 || i == (_sampleCount - 1) ? 0.5 : 1.0;
    
        result += y * weight * dx;
    }
    return result;
}

internal float3 computeTrasmittance(float height, float mu)
{  

    return exp(-(
        Atmosphere.RayleighScattering * opticalLength(Atmosphere.RayleighDensityExpScale, height, mu) +
        Atmosphere.MieExtinction * opticalLength(Atmosphere.MieDensityExpScale, height, mu)
        Atmosphere.OzoneAbsorbtion * opticalLength(Atmosphere.Ozo)
       //ozone
    ));
}



