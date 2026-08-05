#include "common/camera.slang"
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

    uint3 res;
    _hiz.GetDimensions(0, res.x, res.y, res.z);

    float4x4 worldToClip = mul(_camera.projection, _camera.view);
    float4x4 clipToWorld = mul(_camera.invView, _camera.invProj);


    float3 worldPos = _world_position.Load(uint3(screenCoord.xy,0)).xyz;
    float3 cameraRay = normalize(worldPos - _camera.position.xyz);

    float3 normal = _normal.Load(uint3(screenCoord.xy,0)).xyz;

    float3 worldSpaceSampleDir = normalize(reflect(cameraRay, normal));

    if(dot(worldSpaceSampleDir.xyz, -_camera.direction.xyz) < 0) return;

    float roughness = _roughness_metallic.Load(screenCoord).r;
    if(roughness > 0.9) return;


    float4 ndcOrigin = mul(worldToClip, float4(worldPos.xyz,1));
    ndcOrigin.xyz /= ndcOrigin.w;


    float3 sampleDirNorm = cross(worldSpaceSampleDir, normal);
    sampleDirNorm = normalize(cross(sampleDirNorm, worldSpaceSampleDir));


    float4 sampleDir = mul(worldToClip, float4(worldPos.xyz + worldSpaceSampleDir,1));
    sampleDir /= sampleDir.w;
    sampleDir.xyz -= ndcOrigin.xyz;
    sampleDir.xy = normalize(sampleDir.xy);
    float2 toEdge;
    toEdge = saturate(abs(sampleDir.xy * 2 + ndcOrigin.xy));

    float maxAxis = max(toEdge.x, toEdge.y);
    uint stepsToX = toEdge.x * res.x / 2;
    uint stepsToY = toEdge.y * res.y / 2;
    uint steps = max(stepsToX, stepsToY);

    float stepSize = stepsToX > stepsToY ? (toEdge.x / float(steps)) : (toEdge.y / float(steps)) ;
    bool maxAxisX = stepsToX > stepsToY;

    uint mipLevels = res.z;
    int currentMip = 0;
    int i = 1;

    int iter = 0;
    float averageMip;
    while(i < steps){
        iter++;
        averageMip += float(currentMip) / float(mipLevels);
        float2 samplePos = ndcOrigin.xy + sampleDir.xy * i * stepSize;

        if(abs(samplePos.x) > 1 || abs(samplePos.y) > 1 ) return;


        uint2 screenSpacePos = (samplePos.xy * 0.5 + 0.5) * res.xy ;
        float sampleDepth = _hiz.Load(uint3(screenSpacePos / (1 << currentMip), currentMip)).r;

        float4 worldSpaceSamplePos = mul(clipToWorld, float4(samplePos.xy,sampleDepth,1));
        worldSpaceSamplePos.xyz /= worldSpaceSamplePos.w;
        float3 sample = worldSpaceSamplePos.xyz - worldPos.xyz;

        float d = dot(sample, sampleDirNorm);
        float d1 = dot(sample, worldSpaceSampleDir);

        if(d < 0 || d1 < 0)
        {
            i += (1 << currentMip);
            uint previousmip = currentMip;
            currentMip = min(mipLevels - 1, maxAxisX ? firstbitlow(screenSpacePos.x): firstbitlow(screenSpacePos.y));

            continue;
        }

        if(currentMip == 0) {

            if(ndcOrigin.z < sampleDepth)
                break;

            i++;
        }

        currentMip = max(0, currentMip - 1);
    }

   // _hdrOutput.Store(screenCoord.xy, float4(float3((averageMip / iter) < 1.0),1));

    //return;
    if(i >= steps) return;

    float2 samplePos = ndcOrigin.xy + sampleDir.xy * i * stepSize;

    uint2 screenSpacePos = (samplePos.xy * 0.5 + 0.5) * res.xy;

    float4 outputColor = _ssrChain.Load(screenCoord);
    float4 reflectionColor = _ssrChain.Load(uint3(screenSpacePos.xy,0));

    outputColor += reflectionColor *  (1 - roughness);
    _hdrOutput.Store(screenCoord.xy, float4(reflectionColor));


}
