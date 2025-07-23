#include "bindings/lighting.slang"

float linearize();

float shadowPercentage(Sampler2D shadowmap, float3 worldPos) {


    float4 csp = mul(LIGHT_DIRECT_PROJ , mul(LIGHT_DATA[LIGHT_DIRECT_INDEX].trasformation , float4(worldPos,1)));
    float2 uv = (normalize(csp) * 0.5 + 0.5).xy;

    float shadowMapDepth = shadowmap.Sample(uv.xy).r;
    float currentDepth = 

    return  currentDepth < shadowDepth ? 1.0 : 0.0;
float shadowDepth = shadowMap.Sample(someSampler, uv).r;
float currentDepth = linearize(lightSpacePos.z / lightSpacePos.w); // Must match map

}       