#include "bindings/lighting.slang"
#include "bindings/camera.slang"
#include "bindings/environment.slang"
#include "comp/utils.slang"

namespace Shadow{
   
    float4x4 shadowProjectionMatrix() {

      
        
        float right = -1e9, left = 1e9, top = -1e9, bottom = 1e9, far = -1e9;

        for(int i = 0; i< 8; i++){
            float4 point = mul(uLightData.lights[uLightData.directIndex].view, uCamera.frustumBounds[i]);
            
            right = max(right,point.x);
            left = min(left,point.x);
            top= max(top,point.y);
            bottom = min(bottom,point.y);
            far = max(far, point.z);
        }

        // right = min(right, uEnvironment.scene_size);
        // left = max(left, -uEnvironment.scene_size);
        // top = min(top, uEnvironment.scene_size);
        // bottom = max(bottom, -uEnvironment.scene_size);

       // far = uEnvironment.scene_size;
        return orthoProjectionMatrix(top,bottom,left,right, far);
        
    }

float shadowPercentage(Sampler2D shadowmap, float3 worldPos) {

        float4 lightSpacePos = mul(mul(shadowProjectionMatrix() ,uLightData.lights[uLightData.directIndex].view) , float4(worldPos,1));
        float2 uv = lightSpacePos.xy * 0.5 + 0.5;
        
        float shadowMapDepth = shadowmap.Sample(uv.xy).r;
        
        return  (lightSpacePos.z - 0.005) < shadowMapDepth ? 0 : 1;
    }       
}