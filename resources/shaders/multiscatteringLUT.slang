
#include "comp/sky/sky.slang"

[vk_binding(0,2)] Sampler2D<float4> _transmittanceLUT;
[vk_binding(1,2)] WTexture2D<float4> _multiscatteringLUT;




void main(uint3 dispatch : SV_DispatchThreadID) {

    writeMultiscatteringLUT(_multiscatteringLUT,_transmittanceLUT,dispatch.xy);
        
}