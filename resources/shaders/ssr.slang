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
    if(roughness > 0.9) return;


    float3 sampleDir = mul(_camera.view, float4(worldSpaceSampleDir,0)).xyz;
    float3 rayOriginPos = mul(_camera.view, float4(worldPos,1)).xyz;
    float3 samplePos = rayOriginPos + sampleDir * 300;

    float4 geometryAlignedPos;


    uint iter = 0;
    do{

        float4 clipSpacePos = mul(_camera.projection, float4(samplePos,1));
        clipSpacePos.xy /= clipSpacePos.w;
        if(abs(clipSpacePos.x) > 1 || abs(clipSpacePos.y) > 1) return;


        uint2 screenSpacePos = (clipSpacePos.xy * 0.5 + 0.5) * res.xy;


        float sampleDepth = _hiz.Load(uint3(screenSpacePos,0)).r;
        geometryAlignedPos = mul(_camera.invProj,float4(clipSpacePos.xy, sampleDepth,1));
        geometryAlignedPos.xyz /= geometryAlignedPos.w;
        samplePos = sampleDir * dot(geometryAlignedPos.xyz - rayOriginPos, sampleDir) + rayOriginPos;

        iter++;
    } while(abs(geometryAlignedPos.z - samplePos.z) > 1  && iter < 16 );


    float4 clipSpacePos = mul(_camera.projection, float4(samplePos,1));
    clipSpacePos.xyz /= clipSpacePos.w;
    if(abs(clipSpacePos.x) > 1 || abs(clipSpacePos.y) > 1) return;

    uint2 screenSpacePos = (clipSpacePos.xy * 0.5 + 0.5) * res.xy;


    float4 outputColor = _ssrChain.Load(screenCoord);
    float4 reflectionColor = _ssrChain.Load(uint3(screenSpacePos.xy,0));

    outputColor += reflectionColor *  (1 - roughness);
    _hdrOutput.Store(screenCoord.xy, float4(outputColor));

}
