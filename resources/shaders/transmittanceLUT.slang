
#include "comp/sky/sky.slang"

[vk_binding(0,2)] WTexture2D<float4> _transmittanceLUT;



void main(uint3 dispatch : SV_DispatchThreadID) {

    writeTransmittanceLUT(_transmittanceLUT,dispatch.xy);
        
}