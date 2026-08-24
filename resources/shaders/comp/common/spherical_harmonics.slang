module spherical_harmonics;
import comp.common;

constexpr float functionsSH<let L : uint, let m : int>(float3 direction) {
	uint index = L * L + L + m;

	switch (index) {
		case 0:
			return (1.0f / 2.0f) * sqrt(1.0f / PI);	 // L0 m0
		case 1:
			return sqrt(3.0f / (4.0f * PI)) * direction.y;	// L1 m-1
		case 2:
			return sqrt(3.0f / (4.0f * PI)) * direction.z;	// L1 m0
		case 3:
			return sqrt(3.0f / (4.0f * PI)) * direction.x;	// L1 m+1
		case 4:
			return sqrt(15.0f / (4.0f * PI)) * direction.x * direction.y;  // L2 m-2
		case 5:
			return sqrt(15.0f / (4.0f * PI)) * direction.y * direction.z;  // L2 m-1
		case 6:
			return sqrt(5.0f / (16.0f * PI)) * (3.0f * direction.z * direction.z - 1.0f);  // L2 m0
		case 7:
			return sqrt(15.0f / (4.0f * PI)) * direction.x * direction.z;  // L2 m+1
		case 8:
			return sqrt(15.0f / (16.0f * PI)) * (direction.x * direction.x - direction.y * direction.y);  // L2 m+2
		default:
			return 0.0f;
	}
}

public
struct SphericalHarmonic {
	float4 coefficents[9];

public
	__init() {
		coefficents = { float4(0), float4(0), float4(0), float4(0), float4(0),
						float4(0), float4(0), float4(0), float4(0) };
	}

	[mutating] public void addValue(float3 direction, float3 value) {
		coefficents[0].xyz += value * functionsSH<0, 0>(direction);
		coefficents[1].xyz += value * functionsSH<1, -1>(direction);
		coefficents[2].xyz += value * functionsSH<1, 0>(direction);
		coefficents[3].xyz += value * functionsSH<1, 1>(direction);
		coefficents[4].xyz += value * functionsSH<2, -2>(direction);
		coefficents[5].xyz += value * functionsSH<2, -1>(direction);
		coefficents[6].xyz += value * functionsSH<2, 0>(direction);
		coefficents[7].xyz += value * functionsSH<2, +1>(direction);
		coefficents[8].xyz += value * functionsSH<2, +2>(direction);
	}

public
	float3 getValue(float3 direction) {
		float3 value = float3(0);

		value += coefficents[0].xyz * functionsSH<0, 0>(direction);
		value += coefficents[1].xyz * functionsSH<1, -1>(direction);
		value += coefficents[2].xyz * functionsSH<1, 0>(direction);
		value += coefficents[3].xyz * functionsSH<1, 1>(direction);
		value += coefficents[4].xyz * functionsSH<2, -2>(direction);
		value += coefficents[5].xyz * functionsSH<2, -1>(direction);
		value += coefficents[6].xyz * functionsSH<2, 0>(direction);
		value += coefficents[7].xyz * functionsSH<2, +1>(direction);
		value += coefficents[8].xyz * functionsSH<2, +2>(direction);

		return max(0, value);
	}
}
