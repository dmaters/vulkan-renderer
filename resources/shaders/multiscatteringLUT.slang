#include "comp/sky/sky.slang"

[vk_binding(0,2)] uniform Sampler2D _transmittanceLUT;
[vk_binding(1,2)] WTexture2D<float4> _multiscatteringLUT;
[vk_binding(2,2)] RWStructuredBuffer<float> _computeScratchBuffer;



[numthreads(64,1,1)]
void main(uint3 dispatch : SV_DispatchThreadID, uint3 thread : SV_GroupThreadID) {
    writeMultiscatteringLUT(_multiscatteringLUT,_transmittanceLUT,dispatch.xy, thread, _computeScratchBuffer);
        
}