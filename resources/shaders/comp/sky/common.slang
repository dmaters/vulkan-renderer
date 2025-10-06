#pragma once

implementing sky;

internal static const float PI = 3.14159265358979323846f;

internal struct Atmosphere {

    static const float BottomRadius = 6360;
	static const float TopRadius = 6460;


	static const float RayleighDensityExpScale = -1 / 8.0f;
	static const float3 RayleighScattering = float3(0.005802,0.013558,0.0331);

	static const float MieDensityExpScale = -1 / 1.2f;
	static const float3 MieScattering = float3(0.003996);
	static const float3 MieAbsorption = float3(0.00440);
	static const float3 MieExtinction = MieScattering + MieAbsorption;

	static const float OzoneLayerHeight = 25;
	static const float OzoneLayerWidth = 30;
	static const float3 OzoneAbsorbtion = float3(0.000650,0.001881,0.000085);

};



internal struct MediumSample
{
	float3 scattering;
	float3 absorption;
	float3 extinction;

	float3 scatteringMie;
	float3 absorptionMie;
	float3 extinctionMie;
	float3 ozoneAbsorbtion;

	float3 scatteringRay;
	float3 absorptionRay;
	float3 extinctionRay;


	float3 albedo;

	
	static internal MediumSample Get(float height)
	{
		float altitude = height - Atmosphere.BottomRadius;
		float densityMie = exp(Atmosphere.MieDensityExpScale * altitude);
		float densityRay = exp(Atmosphere.RayleighDensityExpScale * altitude);
		float densityOzo = max(0.0, 1.0 - abs(altitude - Atmosphere.OzoneLayerHeight) / (Atmosphere.OzoneLayerWidth / 2.0));
	

		MediumSample s;

		s.scatteringMie = densityMie * Atmosphere.MieScattering;
		s.absorptionMie = densityMie * Atmosphere.MieAbsorption;
		s.extinctionMie = densityMie * Atmosphere.MieExtinction;

		s.scatteringRay = densityRay * Atmosphere.RayleighScattering;
		s.absorptionRay = 0.0f;
		s.extinctionRay = s.scatteringRay + s.absorptionRay;

		s.ozoneAbsorbtion = densityOzo * Atmosphere.OzoneAbsorbtion;
		
		s.scattering = s.scatteringMie + s.scatteringRay;
		s.absorption = s.absorptionMie + s.absorptionRay + s.ozoneAbsorbtion;
		s.extinction = s.extinctionMie + s.extinctionRay + s.ozoneAbsorbtion;
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
	
	if (d < 0.0 || mu >= 0.0) return -1.0;
    return -height * mu - sqrt(d);
}