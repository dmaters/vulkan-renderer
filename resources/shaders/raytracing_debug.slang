#include "common/camera.slang"
#include "common/primitive.slang"
#include "common/static_textures.slang"
import comp.raytracing;

struct PBRmaterial {
	uint32_t albedo_texture;
	uint32_t normal_texture;
	uint32_t metallic_roughness_texture;
	float metallic_value;
	float3 albedo_value;
	float roughness_value;
};

[vk_binding(0)] ConstantBuffer<Camera> _camera;
[vk_binding(1)] RaytracingAccelerationStructure _accellerationStructure;
[vk_binding(2)] StructuredBuffer<uint32_t> _indices;
[vk_binding(3)] StructuredBuffer<PrimitiveData> _primitiveData;
[vk_binding(4)] StructuredBuffer<Vertex::Attributes> _vertexAttributes;
[vk_binding(5)] StructuredBuffer<PBRmaterial> _materialData;

struct VertexOutput {
	float4 position : SV_Position;
	float2 tex_coord : TEXCOORD0;
};

float4 main(VertexOutput vOutput) {
	float2 clipPos = vOutput.tex_coord * 2.0 - 1.0;

	float4 cameraSpaceDir = mul(_camera.invProj, float4(clipPos, 1, 1));
	cameraSpaceDir /= cameraSpaceDir.w;
	float3 direction = mul(_camera.invView, float4(cameraSpaceDir.xyz, 0)).xyz;
	direction = normalize(direction);

	Optional<TraceRayResults> res =
		TraceRay(_camera.position.xyz, direction, _camera.farPlane, _accellerationStructure);

	if (!res.hasValue) return float4(0);

	PrimitiveData primitiveData = _primitiveData[res.value.primitiveID];

	uint3 index = uint3(
		_indices[primitiveData.indexOffset + res.value.baseIndex + 0],
		_indices[primitiveData.indexOffset + res.value.baseIndex + 1],
		_indices[primitiveData.indexOffset + res.value.baseIndex + 2]
	);

	Vertex::Attributes attributes[3] = {
		_vertexAttributes[primitiveData.vertexOffset + index[0]],
		_vertexAttributes[primitiveData.vertexOffset + index[1]],
		_vertexAttributes[primitiveData.vertexOffset + index[2]],
	};

	float2 uv = attributes[0].texCoord * res.value.barycentrics.x + attributes[1].texCoord * res.value.barycentrics.y +
				attributes[2].texCoord * res.value.barycentrics.z;
	PBRmaterial material = _materialData[primitiveData.materialIndex];

	float4 albedo = _staticTextures[material.albedo_texture].Sample(_samplers[SamplerType::LinearRepeat], uv);

	return float4(albedo.xyz, 1);
}
