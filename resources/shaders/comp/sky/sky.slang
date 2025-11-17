module sky;


// The following implementation follows the paper "A Scalable and Production Ready, Sky and Atmosphere Rendering Technique" by Sébastien Hillaire
// The code partly follows the example provided
// For a more accurate implementation please go to the source

import comp.common.spherical_harmonics;
import comp.common;
__include luminance;





public void writeTransmittanceLUT(WTexture2D<float4>transmittanceLUT, uint2 coord) {
    
    uint3 size;
    transmittanceLUT.GetDimensions(0,size.x,size.y,size.z);
     
    float height, mu;
    transmittanceParams(float2(coord) / size.xy, height, mu);

    transmittanceLUT.Store(coord,float4(computeTrasmittance(height,mu),1)); 
}

public float3 readSkyViewLUT(Sampler2D<float4> skyviewLUT, float3 direction) {
 
    float lat = asin(direction.y);            
    float lon = atan2(direction.x, direction.z);      
    float2 uv;
 
    uv.x = lon / PI;
    uv.y = (1.0 + sign(lat) *  sqrt(2.0 * abs(lat) / PI)) / 2;
    uv.x = abs(uv.x);
    return skyviewLUT.Sample(uv).rgb;
}

public void writeSkyViewLUT(WTexture2D<float4> skyviewLUT, Sampler2D<float4> transmittanceLUT, Sampler2D<float4> multitransferLUT, uint2 coord, float3 sunDirection, float sunIntensity){
  
    uint3 dimensions;
    skyviewLUT.GetDimensions(0,dimensions.x,dimensions.y,dimensions.z);
    float2 uv = (float2(coord.xy) / dimensions.xy);
    float lon = (uv.x * 2.0 - 1) * PI / 2;
    float t = uv.y * 2 - 1;   
    float lat = sign(t) * t * t * PI / 2;

    float3 direction = float3(
        cos(lon) * cos(lat),
        sin(lat),
        cos(lat) * sin(lon)
    );
    
    float height = Atmosphere.BottomRadius + 0.1;
    float3 luminance = getScatteredLuminance<AnisotropicPhase,LUTMultiscatteringProvider,true,false>(transmittanceLUT,height,direction, sunDirection, 1, {multitransferLUT}).luminance ;
    skyviewLUT.Store(coord,float4(luminance,1));
}

public void writeMultiscatteringLUT(WTexture2D<float4> multiscatteringLUT, Sampler2D<float4> transmittanceLUT, uint2 coord) {

    uint3 dimensions;
    multiscatteringLUT.GetDimensions(0,dimensions.x,dimensions.y,dimensions.z);
    float2 uv = (coord.xy + float2(0.5)) / dimensions.xy;
    
    float muS;
    float height;
    getMultipleScatteringParams(uv, muS, height);

    
    float3 sunDir = normalize(float3(0, muS, sqrt(1.0 - muS * muS)));

    float3 multiscattering = computeMultipleScattering(transmittanceLUT,height,sunDir);
    multiscatteringLUT.Store(coord,float4(multiscattering,1));
}