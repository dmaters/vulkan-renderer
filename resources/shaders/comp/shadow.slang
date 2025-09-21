#include "bindings/lighting.slang"
#include "bindings/camera.slang"
#include "bindings/environment.slang"
#include "comp/utils.slang"

namespace Shadow{

    static const int CASCADE_BOUNDS[3] = {
        50, 500, 2000
    };

    float4x4 shadowProjectionMatrix(uint cascade) {

            
        float right = -1e9, left = 1e9, top = -1e9, bottom = 1e9, near = -1e9, far = 1e9;
        float3[8] frustumPoints;

        for(int i = 0; i < 4; i++) {
            frustumPoints[i] = uCamera.frustumPlanes[cascade][i].xyz;
            frustumPoints[i + 4] = uCamera.frustumPlanes[cascade+1][i].xyz;

        }

        for(int i = 0; i< 8; i++){
            float4 point = mul(uLightData.lights[uLightData.directIndex].view, float4(frustumPoints[i],1) );           

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
        
        return orthoProjectionMatrix(left, right, bottom, top , near, far);
      
    }

float shadowPercentage(Sampler2D shadowmap, float3 worldPos) {
        float distance = length(mul(uCamera.view,float4(worldPos,1)).xyz);

        if(distance > CASCADE_BOUNDS[2]) return 1;

        uint cascade = distance < CASCADE_BOUNDS[1] ? 1 : 2;
        cascade = distance < CASCADE_BOUNDS[0] ? 0 : cascade;

        float4 lightSpacePos = mul(mul(shadowProjectionMatrix(cascade) ,uLightData.lights[uLightData.directIndex].view), float4(worldPos,1));
        lightSpacePos.y = -lightSpacePos.y;
      
        float2 uv = lightSpacePos.xy * 0.5 + 0.5;

        float shadowMapDepth = shadowmap.Sample(uv.xy).r;
        
        //return shadowMapDepth;
        return  (lightSpacePos.z - 0.005) < shadowMapDepth ? 0 : 1;
    }       
}