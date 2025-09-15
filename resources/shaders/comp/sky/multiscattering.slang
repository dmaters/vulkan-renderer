implementing sky;
__include common;
__include luminance;
// lsecord ()
// psi() = l2ord(samplePos, -cosViewAzi) * aniPhase

float3 ComputeMultipleScattering(float height, float mu, float mu_s, float nu) {

  // Number of intervals for the numerical integration.
  static const int _sampleCount = 50;
  // The integration step, i.e. the length of each integration interval.
  float dx = distanceToAtmosphere(height, mu) / _sampleCount;
  // Integration loop.
  float3 rayleighMieSum = float3(0.0);
  for (int i = 0; i <= _sampleCount; ++i) {
    float di = i * dx;

    // The r, mu and mu_s parameters at the current integration point (see the
    // single scattering section for a detailed explanation).
    float ri = clamp(
		sqrt(di * di + 2.0 *  height * mu * di + height * height), 
		Atmosphere.BottomRadius,
		Atmosphere.TopRadius
	);
    
	float mui = clamp((height * mu + di) / ri, -1, 1);
    float musi = clamp((height * mu_s + di * nu) / ri, -1, 1);

    // The Rayleigh and Mie multiple scattering at the current sample point.
    float3 rayleighMie =
        GetScattering(
            atmosphere, scattering_density_texture, r_i, mu_i, mu_s_i, nu,
            ray_r_mu_intersects_ground) *
        GetTransmittance(
            atmosphere, transmittance_texture, r, mu, d_i,
            ray_r_mu_intersects_ground) *
        dx;
    // Sample weight (from the trapezoidal rule).
    float weight = (i == 0 || i == _sampleCount) ? 0.5 : 1.0;
    rayleigh_mie_sum += rayleighMie * weight;
  }
  return rayleigh_mie_sum;
}


internal float3 getMultipleScattering(float3 scattering, float3 extinction, float altitude, float viewZenithCosAngle)
{

    Sampler2D<float3> _multiScatterLUT;

	float2 uv = saturate(float2(viewZenithCosAngle*0.5f + 0.5f, (altitude) / (Atmosphere.TopRadius - Atmosphere.BottomRadius)));
	//uv = float2(fromUnitToSubUvs(uv.x, MultiScatteringLUTRes), fromUnitToSubUvs(uv.y, MultiScatteringLUTRes));

	float3 multiScatteredLuminance = _multiScatterLUT.Sample(uv).rgb;
	return multiScatteredLuminance;
}

internal float3 computeMultipleScattering(float distanceToPoint, float viewAziCos){

}