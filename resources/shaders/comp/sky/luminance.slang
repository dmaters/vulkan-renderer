implementing sky;
import comp.common;
__include atmosphere;
__include multiscattering;
__include transmittance;

interface IPhaseProvider {
	static float rayleighPhase(float nu);
	static float miePhase(float nu);
}

internal struct AnisotropicPhase : IPhaseProvider {
	// Cornette Shanks
	static float miePhase(float cosTheta) {
		static const float _g = .8;

		float k = 3.0 / (8.0 * PI) * (1.0 - _g * _g) / (2.0 + _g * _g);
		return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + _g * _g - 2.0 * _g * cosTheta, 1.5);
	}
	static float rayleighPhase(float cosTheta) {
		float factor = 3.0f / (16.0f * PI);
		return factor * (1.0f + cosTheta * cosTheta);
	}
};

internal struct IsotropicPhase : IPhaseProvider {
	static float miePhase(float cosTheta) { return 1.0 / (4.0 * PI); }
	static float rayleighPhase(float cosTheta) { return 1.0 / (4.0 * PI); }
}

interface IMultiscatteringProvider {
	float3 getMultiscattering(float height, float muS);
}
struct DummyMultiscatteringProvider : IMultiscatteringProvider {
	float3 getMultiscattering(float height, float muS) { return float3(0); }
}

struct LUTMultiscatteringProvider : IMultiscatteringProvider {
	uniform Texture2D multiscatteringLUT;
	float3 getMultiscattering(float height, float muS) {
		float2 uv;
		getMultipleScatteringUV(muS, height, uv);
		return multiscatteringLUT.SampleLevel(_samplers[SamplerType::LinearClamp], uv, 0).rgb;
	}
}

struct ScatteredLuminanceResult {
	float3 luminance;

	// Additional parameter computed for multiscattering
	float3 luminanceFactor;
};
internal ScatteredLuminanceResult getScatteredLuminance<
	Phase : IPhaseProvider,
	Multiscattering : IMultiscatteringProvider,
	let Shadow : bool,
	let Ground : bool>(
	uniform Texture2D transmittanceLUT,
	float height,
	float3 direction,
	float3 sunDirection,
	float3 sunIlluminance = 1,
	Multiscattering multiscatteringProvider = {}
) {
	static const uint _sampleCount = 64;
	float mu = dot(direction, float3(0, 1, 0));
	float nu = dot(sunDirection, direction);

	float dg = distanceToGround(height, mu);
	float da = distanceToAtmosphere(height, mu);
	float distance = dg > 0 ? min(dg, da) : da;
	float dt = distance / float(_sampleCount);

	float t = 0.0f;

	float miePhase = Phase::miePhase(nu);
	float rayleighPhase = Phase::rayleighPhase(nu);
	float3 throughput = 1.0;
	float3 L = float3(0);
	float3 f = float3(0);
	const float sampleOffset = 0.3f;

	for (uint i = 0; i < _sampleCount; i++) {
		float NewT = distance * (i + sampleOffset) / _sampleCount;
		dt = NewT - t;
		t = NewT;
		float3 p = float3(0.0, height, 0.0) + direction * t;
		float pHeight = length(p);
		float3 up = normalize(p);
		float muS = dot(sunDirection, up);

		MediumSample sample = MediumSample::Get(pHeight);
		const float3 T = exp(-sample.extinction * dt);
		float3 transmittanceToSun = getTransmittance(transmittanceLUT, pHeight, muS);
		float3 phaseScattering = miePhase * sample.scatteringMie + rayleighPhase * sample.scatteringRay;
		float3 shadow = 1.0 - saturate(sign(distanceToGround(pHeight, muS)));
		float3 MS = multiscatteringProvider.getMultiscattering(pHeight, muS);
		float3 S = sunIlluminance * transmittanceToSun * shadow * phaseScattering + MS * sample.scattering;
		float3 Sint = (S - S * T) / sample.extinction;

		float3 Sf = sample.scattering;
		float3 Sfint = (Sf - Sf * T) / sample.extinction;
		f += throughput * Sfint;

		if (Ground && dg > 0 && (i == _sampleCount - 1)) {
			float NdL = saturate(dot(up, sunDirection));

			L += sunIlluminance * transmittanceToSun * throughput * NdL * GroundAlbedo / PI;
		}

		L += throughput * Sint;
		throughput *= T;
	}

	return { L, f };
}
