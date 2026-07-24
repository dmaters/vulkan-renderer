#include "common/camera.slang"
[vk_binding(0)] ConstantBuffer<Camera> _camera;
[vk_binding(1)] Texture2D _normal;
[vk_binding(2)] Texture2D _world_position;
[vk_binding(3)] Texture2D _roughness_metallic;
[vk_binding(4)] Texture2D _ssrChain;
[vk_binding(5)] Texture2D _hiz;

[vk_binding(6)] WTexture2D _hdrOutput;

[numthreads(1,1,1)]
[shader("compute")]



void main(uint3 screenCoord : SV_GroupID){

    uint3 res;
    _hdrOutput.GetDimensions(0, res.x, res.y, res.z);

    float4x4 clipToWorld = mul(_camera.invView, _camera.invProj);
    float4x4 worldToClip = mul(_camera.projection, _camera.view);
    float3 worldPos = _normal.Load(uint3(screenCoord.xy,0)).xyz;
    float3 cameraRay = normalize(worldPos - transpose(_camera.invView)[3].xyz);

    float3 normal = _world_position.Load(uint3(screenCoord.xy,0)).xyz;
    float3 worldSpaceSampleDir = normalize(reflect(cameraRay, normal));
    float reflectionPointDepth = _hiz.Load(screenCoord).r;
    float roughness = _roughness_metallic.Load(screenCoord).r;

    int lowerHiZmip = log2(min(res.x,res.y));
    int currentHiZmip = lowerHiZmip;

    float4 clipSpaceSampleDir = mul(worldToClip,float4(worldSpaceSampleDir,0));
    clipSpaceSampleDir.xyz /= clipSpaceSampleDir.w;

    float2 mip0Sizes =  float2(1) / float2(res.xy);
    float mip0SampleSize = max(mip0Sizes.x, mip0Sizes.y) ;
    float2 sampleDir = normalize(clipSpaceSampleDir.xy);
    float2 rayOrigin = float2(screenCoord.xy) / float2(res.xy) * 2.0f - 1.0f;
    float2 samplePos = rayOrigin;

    float distance = 0;

    while(abs(samplePos.x) < 1 && abs(samplePos.y) < 1 ){


        samplePos = rayOrigin + sampleDir * distance;

       float2 ndc = samplePos.xy * 0.5 + 0.5;
       uint2 screenSpaceSamplePos = ndc.xy * ((res.xy) / (1 << currentHiZmip));
       float depth = _hiz.Load(uint3(screenSpaceSamplePos,currentHiZmip)).r;

       if(samplePos.z >= depth && depth >= reflectionPointDepth){

           if(currentHiZmip > 0){
               currentHiZmip = max(0, currentHiZmip - 1);
               continue;
           }
           float4 outputColor = _ssrChain.Load(screenCoord);
           float4 reflectionColor = _ssrChain.Load(uint3(screenSpaceSamplePos.xy,0));

           outputColor += reflectionColor *  (1 - roughness);
           _hdrOutput.Store(screenCoord.xy, float4(outputColor));

           return;
       }


       bool isCurrentLevelSampling = (((sampleCount + 1) << currentHiZmip) % (1 << (currentHiZmip + 1))) > 0;

        if(isCurrentLevelSampling || currentHiZmip == lowerHiZmip) sampleCount += 1 << currentHiZmip;
        else{
            currentHiZmip += 1;
            sampleCount += 1 << (currentHiZmip - 1);
        }

   }


}
