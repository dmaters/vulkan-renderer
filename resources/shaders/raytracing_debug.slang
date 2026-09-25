#include "common/camera.slang"

[vk_binding(0)] ConstantBuffer<Camera> _camera;
[vk_binding(1)] RaytracingAccelerationStructure _accellerationStructure;

struct VertexOutput {
	float4 position : SV_Position;
	float2 tex_coord : TEXCOORD0;
};

uint hashPCG(uint v) {
	uint state = v * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

float3 randomColor(uint seed) {
	uint h = hashPCG(seed);
	return float3((h & 0xFFu), ((h >> 8u) & 0xFFu), ((h >> 16u) & 0xFFu)) / 255.0;
}
float4 main(VertexOutput vOutput) {
	float2 clipPos = vOutput.tex_coord * 2.0 - 1.0;

	float4 cameraSpaceDir = mul(_camera.invProj, float4(clipPos, 1, 1));
	cameraSpaceDir /= cameraSpaceDir.w;
	float3 direction = mul(_camera.invView, float4(cameraSpaceDir.xyz, 0)).xyz;
	direction = normalize(direction);
	RayQuery<RAY_FLAG_NONE> query;
	RayDesc desc;
	desc.Direction = direction;
	desc.Origin = _camera.position.xyz;
	desc.TMin = 0;
	desc.TMax = _camera.farPlane;
	query.TraceRayInline(_accellerationStructure, RAY_FLAG_NONE, 0xFF, desc);
	while (query.Proceed());
	if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
		uint id = query.CommittedPrimitiveIndex();
		return float4(randomColor(id), 1);
	}
	return float4(0, 0, 0, 1);
}
