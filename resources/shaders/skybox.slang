#include "bindings/camera.slang"
#include "comp/sky/sky.slang"

[vk::binding(0,2)] uniform Sampler2D _skyview;


struct VertexOutput {
    float4 position : SV_Position;
    float2 frag_position : TEXCOORD0;
}



[shader("fragment")]
float4 main(VertexOutput input) : SV_Target{

    float2 clipPos = input.frag_position * 2.0 - 1.0;
    float4 cameraSpaceDir = mul(uCamera.invProj, float4(clipPos / 10,1,1));

    cameraSpaceDir /= cameraSpaceDir.w;
    float3 direction = mul(uCamera.invView,cameraSpaceDir).xyz;
    
    return float4(readSkyViewLUT(_skyview,direction),1);
}
