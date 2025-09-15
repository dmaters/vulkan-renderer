#pragma once

implementing sky;

internal static const float PI = 3.14159265358979323846f;


internal struct Atmosphere {
   	// Radius of the planet (center to ground)
    static const float BottomRadius;
	// Maximum considered atmosphere height (center to atmosphere top)
	static const float TopRadius;

	// Rayleigh scattering exponential distribution scale in the atmosphere
	static const float RayleighDensityExpScale;
	// Rayleigh scattering coefficients
	static const float3 RayleighScattering;

	// Mie scattering exponential distribution scale in the atmosphere
	static const float MieDensityExpScale;
	// Mie scattering coefficients
	static const float3 MieScattering;
	// Mie extinction coefficients
	static const float3 MieExtinction;
	// Mie absorption coefficients
	static const float3 MieAbsorption;



	static const float AbsorptionDensity0LayerWidth;
	static const float AbsorptionDensity0ConstantTerm;
	static const float AbsorptionDensity0LinearTerm;
	static const float AbsorptionDensity1ConstantTerm;
	static const float AbsorptionDensity1LinearTerm;
	static const float3 AbsorptionExtinction;

};



internal struct MediumSample
{
	float3 scattering;
	float3 absorption;
	float3 extinction;

	float3 scatteringMie;
	float3 absorptionMie;
	float3 extinctionMie;

	float3 scatteringRay;
	float3 absorptionRay;
	float3 extinctionRay;

	float3 scatteringOzo;
	float3 absorptionOzo;
	float3 extinctionOzo;

	float3 albedo;

	
	static internal MediumSample Get(float height)
	{
		const float densityMie = exp(Atmosphere.MieDensityExpScale * height);
		const float densityRay = exp(Atmosphere.RayleighDensityExpScale * height);
		const float densityOzo = saturate(height < Atmosphere.AbsorptionDensity0LayerWidth ?
			Atmosphere.AbsorptionDensity0LinearTerm * height + Atmosphere.AbsorptionDensity0ConstantTerm :
			Atmosphere.AbsorptionDensity1LinearTerm * height + Atmosphere.AbsorptionDensity1ConstantTerm);

		MediumSample s;

		s.scatteringMie = densityMie * Atmosphere.MieScattering;
		s.absorptionMie = densityMie * Atmosphere.MieAbsorption;
		s.extinctionMie = densityMie * Atmosphere.MieExtinction;

		s.scatteringRay = densityRay * Atmosphere.RayleighScattering;
		s.absorptionRay = 0.0f;
		s.extinctionRay = s.scatteringRay + s.absorptionRay;

		s.scatteringOzo = 0.0;
		s.absorptionOzo = densityOzo * Atmosphere.AbsorptionExtinction;
		s.extinctionOzo = s.scatteringOzo + s.absorptionOzo;

		s.scattering = s.scatteringMie + s.scatteringRay + s.scatteringOzo;
		s.absorption = s.absorptionMie + s.absorptionRay + s.absorptionOzo;
		s.extinction = s.extinctionMie + s.extinctionRay + s.extinctionOzo;
		s.albedo = s.scattering / max(0.001, s.extinction);

		return s;
	}

};



internal float distanceToAtmosphere(float height, float mu){
    float d = height * height * (mu * mu - 1.0) + Atmosphere.TopRadius * Atmosphere.TopRadius;
    return -height * mu - sqrt(d);
}


internal float distanceToGround(float height, float mu){
    float d = height * height * (mu * mu - 1.0) + Atmosphere.BottomRadius * Atmosphere.BottomRadius;
    return -height * mu - sqrt(d);
}

internal bool intersectGround(float height, float mu) {
    return height * height * (mu * mu * - 1.0) + Atmosphere.BottomRadius * Atmosphere.BottomRadius >= 0;
}






