#include "common/camera.slang"
#include "common/static_textures.slang"
#include "comp/pbr.slang"

[vk_binding(0)] ConstantBuffer<Camera> _camera;
[vk_binding(1)] Texture2D _world_position;
[vk_binding(2)] Texture2D _normal;
[vk_binding(3)] Texture2D _roughness_metallic;
[vk_binding(4)] Texture2D _hdrCopy;
[vk_binding(5)] Texture2D _hiz;

[vk_binding(6)] WTexture2D _hdrOutput;

[numthreads(8, 8, 1)][shader("compute")] void main(uint3 screenCoord : SV_DispatchThreadID) {
	uint3 res;
	_hiz.GetDimensions(0, res.x, res.y, res.z);

	if (screenCoord.x >= res.x || screenCoord.y >= res.y) return;

	if (_hiz.Load(uint3(screenCoord.xy, 0)).r == 1) return;

	float4x4 worldToClip = mul(_camera.projection, _camera.view);
	float4x4 clipToWorld = mul(_camera.invView, _camera.invProj);

	float3 normal = _normal.Load(uint3(screenCoord.xy, 0)).xyz;
	float roughness = _roughness_metallic.Load(uint3(screenCoord.xy, 0)).r;

	if (roughness > 0.9) return;

	float3 worldPos = _world_position.Load(uint3(screenCoord.xy, 0)).xyz;
	float4 ndcOrigin = mul(worldToClip, float4(worldPos.xyz, 1));
	ndcOrigin.xyz /= ndcOrigin.w;

	float3 worldSpaceDir = reflect(normalize(worldPos.xyz - _camera.position.xyz), normal.xyz);

	float4 screenSpaceRay = mul(worldToClip, float4(worldPos.xyz + worldSpaceDir.xyz * 100.0f, 1));
	screenSpaceRay.xyz /= screenSpaceRay.w;
	screenSpaceRay.xyz -= ndcOrigin.xyz;

	float3 screenSpaceDir = normalize(screenSpaceRay.xyz);

	float3 bounds = sign(screenSpaceRay.xyz);
	bounds.z = clamp(bounds.z, 0, 1);

	float3 t3 = (bounds - ndcOrigin.xyz) / screenSpaceDir;
	float t = min3(abs(t3.x), abs(t3.y), abs(t3.z));

	uint2 stepsToAxis = uint2(ceil(abs(screenSpaceDir.xy * t)) * 0.5 * res.xy);
	uint steps = max(stepsToAxis.x, stepsToAxis.y);

	float3 rayStep = (screenSpaceDir.xyz * t) / float3(steps);
	int mipLevels = res.z;

	int scaledAxisIndex = t3[0] == t ? 0 : (t3[1] == t ? 1 : 2);

	float scaledAxisRatio = (screenSpaceDir[scaledAxisIndex] * t) / screenSpaceRay[scaledAxisIndex];
	float dW = (screenSpaceRay.w - ndcOrigin.w) * scaledAxisRatio / float(steps);

	int i = 8;
	int currentMip = 0;

	int backtrackingHead = 1;
	int backtrackingTail = 1;
	int iter = 0;

	while (i < steps && iter < (steps * 2)) {
		iter++;

		float3 samplePos = ndcOrigin.xyz + rayStep.xyz * i;

		uint2 screenSpacePos = uint2((samplePos.xy * 0.5 + 0.5) * res.xy);

		float bufferDepth = _hiz.Load(uint3(screenSpacePos >> currentMip, currentMip)).r;
		bool hit = bufferDepth < samplePos.z;

		if (!hit) {
			backtrackingTail = i;

			uint di = (1 << currentMip);
			i += di;

			if (backtrackingHead < backtrackingTail) {
				currentMip = min(mipLevels - 1, currentMip + 1);
				currentMip = min(currentMip, firstbithigh(steps - i - 1));
			}

			continue;
		}

		if (currentMip == 0 && abs(backtrackingHead - backtrackingTail) < 2) {
			if (abs(bufferDepth - samplePos.z) > (0.001 / abs(ndcOrigin.w + dW * i))) return;
			break;
		}

		backtrackingHead = i;
		i = backtrackingTail + (backtrackingHead - backtrackingTail + 1) / 2;
		currentMip = max(0, currentMip - 1);
	}

	if (iter > steps) return;
	if (i >= steps) return;

	float2 samplePos = ndcOrigin.xy + rayStep.xy * i;

	float2 screenSpacePos = (samplePos.xy * 0.5 + 0.5);

	float4 outputColor = _hdrCopy.Load(uint3(screenCoord.xy, 0));

	float4 reflectionColor = _hdrCopy.SampleLevel(_samplers[SamplerType::LinearClamp], screenSpacePos.xy, 0);

	outputColor += reflectionColor * (1 - roughness);
	_hdrOutput.Store(screenCoord.xy, outputColor);
}
