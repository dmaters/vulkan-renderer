module sample;
import math.common;

public float3 fibonacciSamplingSphere(uint i, uint N) {
	static const float gR = PI * (sqrt(5) + 1.0);
	float theta = i * gR;
	float phi = 0;

	phi = 1.0 - 2.0 * i / (N - 1);
	phi = acos(clamp(phi, -1.0, 1.0));

	float3 dir;

	dir.x = cos(theta) * sin(phi);
	dir.y = cos(phi);
	dir.z = sin(theta) * sin(phi);

	return normalize(dir);
};

public float3 fibonacciSamplingSemisphere(uint i, uint N) {
	static const float gR = PI * (sqrt(5) + 1.0);
	float theta = i * gR;
	float phi = 0;

	phi = 1.0 - 1.0 * i / (N - 1);
	phi = acos(clamp(phi, -1.0, 1.0));

	float3 dir;

	dir.x = cos(theta) * sin(phi);
	dir.y = cos(phi);
	dir.z = sin(theta) * sin(phi);

	return normalize(dir);
};

public float3 cosineWeightedSampling(float2 p){
    float r = sqrt(p.x);
	float phi = p.y * 2 * PI;
	return float3(r * cos(phi), sqrt(1.0 - p.x), r * sin(phi));
};

