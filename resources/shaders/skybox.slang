#include "common/camera.slang"
#include "comp/sky/sky.slang"

[vk_binding(0)] ConstantBuffer<Camera> _camera;
[vk_binding(1)] Texture2D _skyview;

struct VertexOutput {
	float4 position : SV_Position;
	float2 frag_position : TEXCOORD0;
}

	[shader("fragment")] float4
	main(VertexOutput input) :
	SV_Target {
	float2 clipPos = input.frag_position * 2.0 - 1.0;

	float4 cameraSpaceDir = mul(_camera.invProj, float4(clipPos, 1, 1));
	cameraSpaceDir /= cameraSpaceDir.w;
	float3 direction = mul(_camera.invView, cameraSpaceDir).xyz;
	direction = normalize(direction);

	float3 sky = readSkyViewLUT(_skyview, direction);

	sky *= 50.0;

	return float4(sky, 1.0);
}
