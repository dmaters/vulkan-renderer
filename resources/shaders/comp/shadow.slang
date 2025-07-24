#include "bindings/lighting.slang"
#include "constants.slang"


float shadowPercentage(Sampler2D shadowmap, float3 worldPos) {


    float4 lightSpacePos = mul(_LIGHT_DIRECT_PROJECTION , mul(LIGHT_DATA[LIGHT_DIRECT_INDEX].trasformation , float4(worldPos,1)));
    float2 uv = (normalize(lightSpacePos) * 0.5 + 0.5).xy;

    float shadowMapDepth = shadowmap.Sample(uv.xy).r;


    return  (lightSpacePos.z - 0.005) < shadowMapDepth ? 1.0 : 0.0;

}       