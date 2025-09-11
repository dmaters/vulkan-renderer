#pragma once

[vk_binding(0,2)] Texture2D<float4> gHDRTexture ;
[vk_binding(1,2)] WTexture2D<float4> gOutputTexture;




void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 coord = dispatchThreadID.xy;
    float3 hdrColor = gHDRTexture.Load(int3(coord, 0)).rgb;
   
    gOutputTexture.Store(coord,float4(hdrColor, 1.0));
}

