#include "comp/sky/sky.slang"

[vk_binding(0)] Texture2D _transmittanceLUT;
[vk_binding(1)] WTexture2D<float4> _multiscatteringLUT;
[vk_binding(2)] RWStructuredBuffer<float> _computeScratchBuffer;

[NumThreads(64, 1, 1)] void main(
	uint3 dispatch : SV_GroupID, uint3 thread : SV_GroupThreadID
) {
	writeMultiscatteringLUT(
		_multiscatteringLUT,
		_transmittanceLUT,
		dispatch.xy,
		thread.x,
		_computeScratchBuffer
	);
}
