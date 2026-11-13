import definitions.camera;
import definitions.light;
import definitions.primitive;
import static_textures;
import samplers;

import math.common;
import math.raytracing;
import math.random;
import math.sample;

import tech.pbr;

struct PBRmaterial {
	uint32_t albedo_texture;
	uint32_t normal_texture;
	uint32_t metallic_roughness_texture;
	float metallic_value;
	float3 albedo_value;
	float roughness_value;
};

[vk_binding(0)] ConstantBuffer<Camera> _camera;
[vk_binding(1)] ConstantBuffer<Light> _light;
[vk_binding(2)] RaytracingAccelerationStructure _accellerationStructure;
[vk_binding(3)] StructuredBuffer<uint32_t> _indices;
[vk_binding(4)] StructuredBuffer<PrimitiveData> _primitiveData;
[vk_binding(5)] StructuredBuffer<Vertex::Attributes> _vertexAttributes;
[vk_binding(6)] StructuredBuffer<PBRmaterial> _materialData;

[vk_binding(7)] RWTexture2D<float4> _outputTexture;

[vk_push_constant] uint64_t _accumulationFrame;

struct RayData {
	float3 hitPos;
	float3 direction;

	float3 brdf = float3(1);
	float3 radiance = float3(0);

	bool outOfBounds = false;

	enum TraceType {
		Bounce,
		Shadow
	};

	TraceType traceType = TraceType::Bounce;
	bool shadowed = true;
};

static const uint MAX_BOUNCES = 10;

[shader("raygen")] void main_rgen() {
	uint3 texCoords = DispatchRaysIndex();
	uint3 texSize = DispatchRaysDimensions();

	float2 texPos = float2(texCoords.xy) / float2(texSize.xy);

	float2 clipPos = texPos * 2.0 - 1.0;

	float3 position = _camera.position.xyz;

	float4 cameraSpaceDir = mul(_camera.invProj, float4(clipPos, 1, 1));
	cameraSpaceDir /= cameraSpaceDir.w;
	float3 direction = mul(_camera.invView, float4(cameraSpaceDir.xyz, 0)).xyz;
	direction = normalize(direction);

	RayData data = {};
	float3 previousData = _outputTexture.Load(texCoords.xy).xyz;
	for (int i = 0; i < MAX_BOUNCES; i++) {
		RayDesc ray;
		ray.Origin = position;
		ray.Direction = direction;
		ray.TMin = _camera.nearPlane;
		ray.TMax = _camera.farPlane;

		TraceRay<RayData>(_accellerationStructure, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, data);

		if (data.outOfBounds) break;

		position = data.hitPos;
		direction = data.direction;
	}

	float3 sample = data.radiance.xyz;
	float3 result = lerp(previousData, sample, 1.0 / float(_accumulationFrame));
	_outputTexture.Store(texCoords.xy, float4(result, 1.0));
};

float3 reconstructNormal(float2 xy) {
	float z = sqrt(1 - dot(xy, xy));
	return normalize(float3(xy, z));
};

Vertex::Attributes lerpAttributes(Vertex::Attributes vertexAttributes[3], float3 barycentrics) {
	Vertex::Attributes res;

	res.normal = vertexAttributes[0].normal * barycentrics.x + vertexAttributes[1].normal * barycentrics.y +
				 vertexAttributes[2].normal * barycentrics.z;
	res.texCoord = vertexAttributes[0].texCoord * barycentrics.x + vertexAttributes[1].texCoord * barycentrics.y +
				   vertexAttributes[2].texCoord * barycentrics.z;
	res.tangent = vertexAttributes[0].tangent * barycentrics.x + vertexAttributes[1].tangent * barycentrics.y +
				  vertexAttributes[2].tangent * barycentrics.z;
	res.bitangent = vertexAttributes[0].bitangent * barycentrics.x + vertexAttributes[1].bitangent * barycentrics.y +
					vertexAttributes[2].bitangent * barycentrics.z;
	return res;
};

float3 lerpPosition(float3 vertex[3], float3 barycentrics) {
	return vertex[0] * barycentrics.x + vertex[1] * barycentrics.y + vertex[2] * barycentrics.z;
};

[shader("closesthit")] void main_chit(inout RayData data, in BuiltInTriangleIntersectionAttributes attributes) {
	PrimitiveData primitiveData = _primitiveData[InstanceID()];
	uint baseIndex = PrimitiveIndex() * 3;
	uint3 index = uint3(
		_indices[primitiveData.indexOffset + baseIndex + 0],
		_indices[primitiveData.indexOffset + baseIndex + 1],
		_indices[primitiveData.indexOffset + baseIndex + 2]
	);

	float3 barycentrics = unpackBarycentrics(attributes.barycentrics);

	Vertex::Attributes vertexInput = lerpAttributes(
		{
			_vertexAttributes[primitiveData.vertexOffset + index[0]],
			_vertexAttributes[primitiveData.vertexOffset + index[1]],
			_vertexAttributes[primitiveData.vertexOffset + index[2]],
		},
		barycentrics
	);

	PBRmaterial material = _materialData[primitiveData.materialIndex];

	float4 albedo =
		_staticTextures[material.albedo_texture].Sample(_samplers[SamplerType::LinearRepeat], vertexInput.texCoord);

	float2 sampledNormal =
		_staticTextures[material.normal_texture].Sample(_samplers[SamplerType::LinearRepeat], vertexInput.texCoord).rg;
	float3 localNormal = reconstructNormal(sampledNormal * 2.0 - 1.0);
	float3x3 TBN = transpose(float3x3(vertexInput.tangent, vertexInput.bitangent, vertexInput.normal));
	float3 normal = normalize(mul(TBN, localNormal));
	float4 roughnessMetallic = _staticTextures[material.metallic_roughness_texture].Sample(
		_samplers[SamplerType::LinearRepeat], vertexInput.texCoord
	);
	float3x3 TNB = transpose(float3x3(vertexInput.tangent, vertexInput.normal, vertexInput.bitangent));
	uint pixel = DispatchRaysIndex().x + DispatchRaysDimensions().x * DispatchRaysIndex().y;
	Random rnd = Random((_accumulationFrame * 0x87654321u) ^ pixel);
	float3 local = cosineWeightedSampling(float2(rnd.float(), rnd.float()));
	float3 nextDirection = normalize(mul(TNB, local));

	float3 rayOrigin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
	float3 rayDirection = WorldRayDirection();

	RayDesc ray;
	ray.Origin = rayOrigin;
	ray.Direction = _light.direction;
	ray.TMin = _camera.nearPlane;
	ray.TMax = _camera.farPlane;
	data.traceType = RayData::TraceType::Shadow;
	TraceRay<RayData>(
		_accellerationStructure,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
		0xFF,
		0,
		0,
		0,
		ray,
		data
	);
	data.traceType = RayData::TraceType::Bounce;
	if (!data.shadowed) {
		data.shadowed = true;
		data.radiance +=
			data.brdf *
			microfacetBRDF(
				-rayDirection, _light.direction, albedo.xyz, normal, roughnessMetallic.x, roughnessMetallic.y
			) *
			saturate(dot(normal, _light.direction)) * _light.intensity;
	}

	data.brdf *= microfacetBRDF(
					 -rayDirection, nextDirection, albedo.xyz, normal, roughnessMetallic.x, roughnessMetallic.y
				 ) *
				 PI;

	data.direction = nextDirection;
	data.hitPos = rayOrigin;
};

[shader("miss")] void main_miss(inout RayData data) {
	if (data.traceType == RayData::TraceType::Bounce) {
		data.outOfBounds = true;
	} else {
		data.shadowed = false;
	}
}
