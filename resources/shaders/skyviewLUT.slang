
#include "bindings/camera.slang"
#include "comp/sky/sky.slang"
#include "bindings/lighting.slang"


[vk_binding(0,2)] uniform Sampler2D _transmittanceLUT;
[vk_binding(1,2)] uniform Sampler2D _multiscatteringLUT;
[vk_binding(2,2)] WTexture2D<float4> _skyviewLUT;



void main(uint3 dispatch : SV_GroupID) {

    float3 sunDirection = -transpose(uLightData.view)[2].xyz;
    sunDirection.y = -sunDirection.y;
    writeSkyViewLUT(_skyviewLUT,_transmittanceLUT,_multiscatteringLUT,dispatch.xy,sunDirection, uLightData.intensity);

}