#include "bindings/lighting.slang"
#include "bindings/camera.slang"
#include "bindings/environment.slang"
#include "comp/utils.slang"

namespace Shadow{


    float4x4 shadowProjectionMatrix() {

            
        float right = -1e9, left = 1e9, top = -1e9, bottom = 1e9, near = -1e9, far = 1e9;

        for(int i = 1; i< 8; i++){
            float4 point = mul(uLightData.lights[uLightData.directIndex].view, uCamera.frustumBounds[i] );           


            right = max(right,point.x);
            left = min(left,point.x);
            top = max(top,point.y);
            bottom = min(bottom,point.y);
            far = min(far, point.z);
            near = max(near, point.z);
        }
        
        float worldUnitsPerTexelx = (right - left) / 1024.0;
        float worldUnitsPerTexely = (top - bottom) / 1024.0;
        
        left = floor(left / worldUnitsPerTexelx) * worldUnitsPerTexelx;
        bottom = floor(bottom / worldUnitsPerTexely) * worldUnitsPerTexely;
        right = floor(right / worldUnitsPerTexelx)  * worldUnitsPerTexelx;
        top = floor(top / worldUnitsPerTexely) * worldUnitsPerTexely;
        
        return orthoProjectionMatrix(left, right, bottom, top , -near, -far);
      
    }

float shadowPercentage(Sampler2D shadowmap, float3 worldPos) {
     
        float4 lightSpacePos = mul(mul(shadowProjectionMatrix() ,uLightData.lights[uLightData.directIndex].view), float4(worldPos,1));
        lightSpacePos.y = -lightSpacePos.y;
      
        float2 uv = lightSpacePos.xy * 0.5 + 0.5;

        float shadowMapDepth = shadowmap.Sample(uv.xy).r;
        
        //return shadowMapDepth;
        return  (lightSpacePos.z - 0.005) < shadowMapDepth ? 0 : 1;
    }       
}