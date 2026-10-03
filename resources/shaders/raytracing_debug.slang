#include "common/camera.slang"

[vk_binding(0)] ConstantBuffer<Camera> _camera;
[vk_binding(1)] RaytracingAccelerationStructure _accellerationStructure;



struct VertexOutput {
	float4 position : SV_Position;
	float2 tex_coord : TEXCOORD0;
};
float4 main(VertexOutput vOutput){
   	float2 clipPos = vOutput.tex_coord * 2.0 - 1.0;

	float4 cameraSpaceDir = mul(_camera.invProj, float4(clipPos, 1, 1));
	cameraSpaceDir /= cameraSpaceDir.w;
	float3 direction = mul(_camera.invView, cameraSpaceDir).xyz;
	direction = normalize(direction);
	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
	RayDesc desc;
	desc.Direction = direction;
	desc.Origin = _camera.position.xyz;
	desc.TMin = 0;
	desc.TMax = _camera.farPlane;
	query.TraceRayInline(_accellerationStructure,RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,0xFFFF,desc);
	query.CommitNonOpaqueTriangleHit();
	if(query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) return float4(1);
	return float4(1,0,0,1);


}
