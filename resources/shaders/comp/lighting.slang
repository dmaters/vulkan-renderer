
static const float PI = 3.14159265358979323846;

float distributionGGX(float NdotH, float a) {
	float a2 = a * a;

	float f = (NdotH * a2 - NdotH) * NdotH + 1.0;
	return a2 / ( PI * f * f);
}

float3 fresnelSchlick(float HdotV, float3 F0) {
	return F0 + (1.0 - F0) * pow(clamp(1.0 - HdotV, 0.0, 1.0), 5.0);
}

float geometrySchlickGGX(float NdotV, float a) {
	float r = (a + 1.0);
	float k = (r * r) / 8.0;
	return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(float NdotV, float NdotL, float a) {
	return geometrySchlickGGX(NdotV, a) * geometrySchlickGGX(NdotL, a);
}

float3 microfacetBRDF(float2 uv, float3 viewDir, float3 lightDir, float3 albedo, float3 normal, float metallic, float roughness) {
	float3 halfview = normalize(viewDir + lightDir);

	float NdotV = max(dot(normal, viewDir), 0.0);
	float NdotL = max(dot(normal, lightDir), 0.0);
	float NdotH = max(dot(normal, halfview), 0.0);
	float HdotV = max(dot(halfview, viewDir), 0.0);

	float3 F0 = (float3(0.04), albedo, metallic);

	float a = roughness * roughness;

	float NDF = distributionGGX(NdotH, a);
	float3 F = fresnelSchlick(HdotV, F0);
	float V = geometrySmith(NdotV, NdotL, a);

	float3 specular = NDF * F * V;

	float3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo / PI;

	return (diffuse + specular) * NdotL;
}
