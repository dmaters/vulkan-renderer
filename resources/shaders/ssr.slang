#include "common/camera.slang"
#include "common/static_textures.slang"
#include "comp/pbr.slang"

[vk_binding(0)] ConstantBuffer<Camera> _camera;
[vk_binding(1)] Texture2D _world_position;
[vk_binding(2)] Texture2D _normal;
[vk_binding(3)] Texture2D _roughness_metallic;
[vk_binding(4)] Texture2D _ssrChain;
[vk_binding(5)] Texture2D _hiz;

[vk_binding(6)] WTexture2D _hdrOutput;


[numthreads(1,1,1)]
[shader("compute")]
void main(uint3 screenCoord : SV_GroupID){

    if(_hiz.Load(uint3(screenCoord.xy,0)).r == 1) return;

    uint3 res;
    _hiz.GetDimensions(0, res.x, res.y, res.z);

    float4x4 worldToClip = mul(_camera.projection, _camera.view);
    float4x4 clipToWorld = mul(_camera.invView, _camera.invProj);



    float3 normal = _normal.Load(uint3(screenCoord.xy,0)).xyz;
    float roughness = _roughness_metallic.Load(uint3(screenCoord.xy,0)).r;

    if(roughness > 0.9) return;

    float3 worldPos = _world_position.Load(uint3(screenCoord.xy,0)).xyz;
    float4 ndcOrigin = mul(worldToClip, float4(worldPos.xyz,1));
    ndcOrigin.xy /= ndcOrigin.w;

    float4 cameraSpacePos = mul(_camera.view, float4(worldPos,1));
    float4 cameraSpaceNormal = mul(_camera.view, float4(normal,0));

    float4 cameraSpaceDir = reflect(normalize(cameraSpacePos), cameraSpaceNormal);


    float3 sampleDirNorm = cross(cameraSpaceDir.xyz, cameraSpaceNormal.xyz);
    sampleDirNorm = normalize(cross(sampleDirNorm.xyz, cameraSpaceDir.xyz));

    float distance = cameraSpaceDir.z < 0 ?  _camera.farPlane  - (-cameraSpacePos.z) : ((-cameraSpacePos.z) - _camera.nearPlane) ;


    float4 screenSpaceRay = mul(_camera.projection, float4(cameraSpacePos.xyz + cameraSpaceDir.xyz * distance,1));


    float4 adjustedRay = screenSpaceRay;
    adjustedRay.xy /= adjustedRay.w;
    adjustedRay.xyz -= ndcOrigin.xyz;

    adjustedRay /= max3(adjustedRay.x, adjustedRay.y, adjustedRay.z);

    float3 sampleDir = normalize(adjustedRay.xyz);
    float k0 = 1 / ndcOrigin.w;
    float k1 = 1 / adjustedRay.w;


    uint2 stepsToAxis = uint2(ceil(abs(adjustedRay.xy) * 0.5 * res.xy));
    uint steps = max(stepsToAxis.x, stepsToAxis.y);

    bool maxAxisX = stepsToAxis.x > stepsToAxis.y;
    float stepSize = abs(maxAxisX ?  adjustedRay.x : adjustedRay.y) / float(steps);
    float kD = (k1 - k0) / float(steps);

    int mipLevels = res.z;

    int minMip = roughness * (mipLevels - 1);
    minMip = 0;

    int i = 1 << minMip;
    int currentMip = minMip;
    int backtrackingHead = -1;
    int backtrackingTail = -1;

    while(i <  steps){
        float2 samplePos = ndcOrigin.xy + sampleDir.xy * i * stepSize ;
        float sampleDepth = ndcOrigin.z + (adjustedRay.z / float(steps)) * i * kD;
        uint2 screenSpacePos = (samplePos.xy * 0.5 + 0.5) * res.xy ;


        float bufferDepth = _hiz.Load(uint3(screenSpacePos >> currentMip, currentMip)).r;
        bool hit = bufferDepth < sampleDepth;


        if(!hit)
        {
            backtrackingTail = i;
            if(backtrackingHead <= backtrackingTail){
                currentMip = min(mipLevels - 1, firstbitlow( maxAxisX ? screenSpacePos.x :screenSpacePos.y ));
                currentMip = min(currentMip, firstbithigh(steps - i - 1));
            }

            uint di = (1 << currentMip);
            i += di;


            continue;
        }

        if( currentMip <= minMip) {
           // float3 localNormal = _normal.Load(uint3(screenSpacePos.xy,0)).xyz;
           //if(dot(worldSpaceSampleDir, localNormal)>= 0) return;
           //
           // if(sampleDir.z > 0 && sampleDepth < ndcOrigin.z) return;

            break;
        }
        backtrackingHead = i;
        i = backtrackingTail + (backtrackingHead - backtrackingTail + 1) / 2;
        currentMip =  max(minMip, currentMip - 1);
    }

    if(i >= steps) return;

    float2 samplePos = ndcOrigin.xy + sampleDir.xy * i * stepSize;

    float2 screenSpacePos = (samplePos.xy * 0.5 + 0.5) ;
    float4 outputColor = _ssrChain.Load(uint3(screenCoord.xy,0));
    float4 reflectionColor = _ssrChain.SampleLevel(_samplers[SamplerType::LinearClamp], screenSpacePos.xy, minMip);

    outputColor += reflectionColor * (1 - roughness);
    _hdrOutput.Store(screenCoord.xy, outputColor);

}
