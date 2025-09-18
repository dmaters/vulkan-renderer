implementing sky;
__include common;
__include luminance;
// lsecord ()
// psi() = l2ord(samplePos, -cosViewAzi) * aniPhase




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