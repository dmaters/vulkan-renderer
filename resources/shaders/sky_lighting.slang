
import comp.sky.sky;
import comp.common.spherical_harmonics;
import comp.common;

[vk_binding(0)] uniform Texture2D _skyviewLUT;
[vk_binding(1)] RWStructuredBuffer<SphericalHarmonic> _SH;

// TODO: Few problems on defining this in a shared module, keep it duplicate
// until a fix is found
public
float3 fibonacciSampling<let Semisphere : bool>(uint i, uint N) {
	static const float gR = PI * (sqrt(5) + 1.0);
	float theta = i * gR;
	float phiCos = 0;
	if (Semisphere)
		phiCos = 1.0 - 1.0 * i / (N - 1);
	else
		phiCos = 1.0 - 2.0 * i / (N - 1);
	phiCos = clamp(phiCos, -1.0, 1.0);
	float phi = acos(phiCos);

	float3 dir;

	dir.x = cos(theta) * sin(phi);
	dir.y = phiCos;
	dir.z = sin(theta) * sin(phi);

	return normalize(dir);
}

void main(uint3 dispatch : SV_GroupID) {
	SphericalHarmonic sh;

	static const uint _samples = 32;
	static const uint _subSamples = 16;

	for (uint i = 0; i < _samples; i++) {
		float3 baseDir = fibonacciSampling<false>(i, _samples);
		float3 up = baseDir;
		float3 forward = abs(up.z) > 0.9999 ? float3(0, 1, 0) : float3(0, 0, 1);
		float3 right = normalize(cross(baseDir, forward));
		forward = cross(right, up);
		up = cross(right, forward);

		right = normalize(right);
		up = normalize(up);
		forward = normalize(forward);

		float3x3 rotation = { right, up, forward };

		float3 color = float3(0);
		for (uint y = 0; y < _subSamples; y++) {
			float3 direction = fibonacciSampling<true>(y, _subSamples);
			direction = mul(rotation, direction);
			color += readSkyViewLUT(_skyviewLUT, direction) * saturate(dot(direction, up));
		}

		float weight = 4 * PI / (_samples * _subSamples);
		sh.addValue(baseDir, color * weight);
	}

	_SH[0] = sh;
}
