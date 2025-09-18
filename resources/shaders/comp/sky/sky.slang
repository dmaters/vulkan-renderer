//The following implementation references the papers listed below
//https://old.cescg.org/CESCG-2009/papers/PragueCUNI-Elek-Oskar09.pdf
//http://www.klayge.org/material/4_0/Atmospheric/Precomputed%20Atmospheric%20Scattering.pdf
//https://sebh.github.io/publications/egsr2020.pdf


module sky;
__include common;
__include luminance;



void writeTransmittanceLUT(WTexture2D<float4>transmittanceLUT, uint2 coord) {
    
    uint3 size;
    transmittanceLUT.GetDimensions(0,size.x,size.y,size.z);

    Transmittance t = transmittanceP(float2(coord) / size.xy);
    float height = t.height;
    float mu = t.mu;

    transmittanceLUT.Store(coord,float4(computeTrasmittance(height,mu),1));
   //transmittanceLUT.Store(coord,float4(mu,height,0,1));

  
    
}




void writeSkyViewLUT(WTexture2D<float4> skyviewLUT, Sampler2D<float4> transmittanceLUT, uint2 coord, float3 sunDirection){
    
    uint3 dimensions;

    skyviewLUT.GetDimensions(0,dimensions.x,dimensions.y,dimensions.z);

    float2 uv = (float2(coord.xy) / dimensions.xy);
    float lon = (uv.x * 2.0 - 1) * PI;
    float t = uv.y * 2 - 1;   
    float lat = sign(t) * t * t * PI / 2;
    lat = -lat;    

    float3 direction = float3(
        cos(lon) * cos(lat),
        sin(lat),
        cos(lat) * sin(lon)
    );
    
    
    
    float3 luminance = float3(0);
    float height = Atmosphere.BottomRadius + 3;
    //Add ground luminance

    luminance = integrateScatteredLuminance(transmittanceLUT,height,direction,normalize(float3(0,0,-1)));

    skyviewLUT.Store(coord,float4(luminance,1));
}