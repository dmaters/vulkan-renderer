#pragma once

implementing sky;

internal static const float PI = 3.14159265358979323846f;

internal struct Atmosphere {
   	// Radius of the planet (center to ground)
    static const float BottomRadius = 6360;
	// Maximum considered atmosphere height (center to atmosphere top)
	static const float TopRadius = 6460;

	// Rayleigh scattering exponential distribution scale in the atmosphere
	static const float RayleighDensityExpScale = -1 / 8.0f;
	// Rayleigh scattering coefficients
	static const float3 RayleighScattering = float3(0.005802,0.013558,0.0331);

	// Mie scattering exponential distribution scale in the atmosphere
	static const float MieDensityExpScale = -1 / 1.2;
	// Mie scattering coefficients
	static const float3 MieScattering = float3(0.003996);
	// Mie absorption coefficients
	static const float3 MieAbsorption = float3(0.00440);
	// Mie extinction coefficients
	static const float3 MieExtinction = MieScattering + MieAbsorption;


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


	float3 albedo;

	
	static internal MediumSample Get(float height)
	{
		const float densityMie = exp(Atmosphere.MieDensityExpScale * (height - Atmosphere.BottomRadius));
		const float densityRay = exp(Atmosphere.RayleighDensityExpScale * (height - Atmosphere.BottomRadius));
	

		MediumSample s;

		s.scatteringMie = densityMie * Atmosphere.MieScattering;
		s.absorptionMie = densityMie * Atmosphere.MieAbsorption;
		s.extinctionMie = densityMie * Atmosphere.MieExtinction;

		s.scatteringRay = densityRay * Atmosphere.RayleighScattering;
		s.absorptionRay = 0.0f;
		s.extinctionRay = s.scatteringRay + s.absorptionRay;

		

		s.scattering = s.scatteringMie + s.scatteringRay;
		s.absorption = s.absorptionMie + s.absorptionRay;
		s.extinction = s.extinctionMie + s.extinctionRay;
		s.albedo = s.scattering / max(0.001, s.extinction);

		return s;
	}

};



internal float distanceToAtmosphere(float height, float mu){

    float d = height * height * (mu * mu - 1.0) + Atmosphere.TopRadius * Atmosphere.TopRadius;
	return -height * mu + sqrt(d);
}


internal float distanceToGround(float height, float mu){
    
	float d = height * height * (mu * mu - 1.0) + Atmosphere.BottomRadius * Atmosphere.BottomRadius;
	return d < 0 ? -1 : max(-height * mu - sqrt(d),0);
}





