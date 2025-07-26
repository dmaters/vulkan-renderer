#include "bindings/lighting.slang"
#include "bindings/environment.slang"
namespace Shadow{
   
float shadowPercentage(Sampler2D shadowmap, float3 worldPos) {
        
        float4 lightSpacePos = mul(mul(directLightProjectionMatrix(uEnvironment.scene_size) ,uLightData.lights[uLightData.directIndex].trasformation) , float4(worldPos,1));
        float2 uv = lightSpacePos.xy * 0.5 + 0.5;
        
        float shadowMapDepth = shadowmap.Sample(uv.xy).r;
        

        return  (lightSpacePos.z - 0.005) < shadowMapDepth ? 0 : 1;

        
    }       
}