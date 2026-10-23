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

[vk_binding(6)] WTexture2D<float4> _outputTexture;


struct RayData {
    uint2 textureCoords;
};

[shader("raygen")]
void main_rgen() {

    uint3 rayIndex = DispatchRaysIndex();
    uint3 rayGroupDimension = DispatchRaysDimensions();

    float2 screenCoords = float2(rayIndex.xy) / float2(rayGroupDimension.xy);

	float2 clipPos = screenCoords * 2.0 - 1.0;

	float4 cameraSpaceDir = mul(_camera.invProj, float4(clipPos, 1, 1));
	cameraSpaceDir /= cameraSpaceDir.w;
	float3 direction = mul(_camera.invView, float4(cameraSpaceDir.xyz, 0)).xyz;
	direction = normalize(direction);

	RayData data;
	data.textureCoords = rayIndex.xy;
	raytracing::TraceRay<RayData>(
	    _camera.position.xyz,
		direction,
		_camera.farPlane,
		_accellerationStructure,
		data
	);
}

[shader("closesthit")]
void main_chit(inout RayData data, in BuiltInTriangleIntersectionAttributes attributes) {

    PrimitiveData primitiveData = _primitiveData[InstanceID()];
    uint baseIndex = PrimitiveIndex() * 3;
	uint3 index = uint3(
		_indices[primitiveData.indexOffset + baseIndex + 0],
		_indices[primitiveData.indexOffset + baseIndex + 1],
		_indices[primitiveData.indexOffset + baseIndex + 2]
	);

	Vertex::Attributes vertexAttributes[3] = {
		_vertexAttributes[primitiveData.vertexOffset + index[0]],
		_vertexAttributes[primitiveData.vertexOffset + index[1]],
		_vertexAttributes[primitiveData.vertexOffset + index[2]],
	};

	float3 barycentrics = raytracing::UnpackBarycentrics(attributes.barycentrics);

	float2 uv = vertexAttributes[0].texCoord * barycentrics.x + vertexAttributes[1].texCoord * barycentrics.y +
				vertexAttributes[2].texCoord * barycentrics.z;
	PBRmaterial material = _materialData[primitiveData.materialIndex];

	float4 albedo = _staticTextures[material.albedo_texture].Sample(_samplers[SamplerType::LinearRepeat], uv);
	_outputTexture.Store(data.textureCoords, float4(albedo.xyz,1));

}


[shader("miss")]
void main_miss(inout RayData data) {
    _outputTexture.Store(data.textureCoords, float4(0,0,0,1));
}



