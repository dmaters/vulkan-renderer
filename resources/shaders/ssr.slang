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


    float3 worldPos = _world_position.Load(uint3(screenCoord.xy,0)).xyz;
    float3 cameraRay = normalize(worldPos - _camera.position.xyz);

    float3 normal = _normal.Load(uint3(screenCoord.xy,0)).xyz;

    float3 worldSpaceSampleDir = normalize(reflect(cameraRay, normal));

    float roughness = _roughness_metallic.Load(uint3(screenCoord.xy,0)).r;

    if(roughness > 0.9) return;


    float4 ndcOrigin = mul(worldToClip, float4(worldPos.xyz,1));
    ndcOrigin.xyz /= ndcOrigin.w;


    float3 sampleDirNorm = cross(worldSpaceSampleDir, normal);
    sampleDirNorm = normalize(cross(sampleDirNorm, worldSpaceSampleDir));


    float4 sampleDir = mul(worldToClip, float4(worldPos.xyz + worldSpaceSampleDir,1));
    sampleDir /= sampleDir.w;
    sampleDir.xyz -= ndcOrigin.xyz;
    sampleDir.xy = normalize(sampleDir.xy);
    float2 toEdge = abs(-sampleDir.xy / max(abs(sampleDir.x), abs(sampleDir.y)) + ndcOrigin.xy);


    float2 edgePos = sign(sampleDir.xy);
    float2 t2 = ((edgePos - ndcOrigin.xy) / sampleDir.xy);
    float t = min(abs(t2.x), abs(t2.y));
    uint2 stepsToAxis = uint2(ceil(abs(sampleDir.xy * t) * 0.5 * res.xy));
    uint steps = max(stepsToAxis.x, stepsToAxis.y);

    bool maxAxisX = stepsToAxis.x > stepsToAxis.y;
    float stepSize =  t / float(steps);

    int mipLevels = res.z;

    int minMip = roughness * (mipLevels - 1);

    int currentMip = minMip;

    int i = 1;
    int backtrackingHead = -1;
    int backtrackingTail = -1;

    bool toInfinity = false;

    while(i < steps ){
        float2 samplePos = ndcOrigin.xy + sampleDir.xy * i * stepSize;
        if(abs(samplePos.x) > 1 || abs(samplePos.y) > 1 ) return;

        uint2 screenSpacePos = (samplePos.xy * 0.5 + 0.5) * res.xy ;


        float sampleDepth = _hiz.Load(uint3(screenSpacePos / (1 << currentMip), currentMip)).r;

        float4 worldSpaceSamplePos = mul(clipToWorld, float4(samplePos.xy,sampleDepth,1));
        worldSpaceSamplePos.xyz /= worldSpaceSamplePos.w;
        float3 sample = worldSpaceSamplePos.xyz - worldPos.xyz;

        float d = dot(sample, sampleDirNorm) ;
        float d1 = dot(sample, worldSpaceSampleDir) ;
        bool backtracking = false;


        if(d < 0 || d1 < 0 )
        {
            backtrackingTail = i;
            uint di = (1 << currentMip);
            i += di;

            if(backtrackingHead <= backtrackingTail){
                currentMip = min(mipLevels - 1, maxAxisX ? firstbitlow(screenSpacePos.x + di ) : firstbitlow(screenSpacePos.y + di ) + minMip);
                currentMip = min(currentMip, firstbithigh(steps - i - 1) + minMip);
            }
            continue;
        }

        if( currentMip <= minMip) {
            float3 localNormal = _normal.Load(uint3(screenSpacePos.xy,0)).xyz;
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
