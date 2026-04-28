#include "common/static_textures.slang"

[vk_binding(0)] Texture2D _image;

static const float3 cLUMA =  float3(0.2126, 0.7152, 0.0722);


[shader("fragment")] float4 main(float2 uv){
    uv.y = 1 - uv.y;
    uint3 imageRes;
    _image.GetDimensions(0,imageRes.x,imageRes.y,imageRes.z);
    float2 texSize = 1.0 / imageRes.xy;


    float3 sampleC = _image.Sample(_samplers[SamplerType::LinearClamp],uv).rgb;
    float3 sampleNE = _image.Sample(_samplers[SamplerType::LinearClamp], uv + float2(0.5,0.5) * texSize).rgb;
    float3 sampleSE = _image.Sample(_samplers[SamplerType::LinearClamp], uv + float2(0.5,-0.5) * texSize).rgb;
    float3 sampleNW = _image.Sample(_samplers[SamplerType::LinearClamp], uv + float2(-0.5,0.5) * texSize).rgb;
    float3 sampleSW = _image.Sample(_samplers[SamplerType::LinearClamp], uv + float2(-0.5,-0.5) * texSize).rgb;


    float lumaC = dot(sampleC, cLUMA);
    float lumaNE = dot(sampleNE, cLUMA);
    float lumaSE = dot(sampleSE, cLUMA);
    float lumaNW = dot(sampleNW, cLUMA);
    float lumaSW = dot(sampleSW, cLUMA);


    float2 dir = float2( (lumaNE + lumaSE) - (lumaNW + lumaSW),(lumaNW + lumaNW) - (lumaSW + lumaSW));
    float dirReduce = max((lumaNE + lumaNW + lumaSE + lumaSW) * (1.0/8.0),1.0/128.0);
    float rcpDir = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDir, -8.0, 8.0) * texSize;

    float3 sampleA = 0.5 * (
        _image.Sample(_samplers[SamplerType::LinearClamp], uv - dir * (1.0/6.0)).rgb +
            _image.Sample(_samplers[SamplerType::LinearClamp], uv + dir * (1.0/6.0)).rgb);

    float3 sampleB = sampleA * 0.5 + 0.25 * (
            _image.Sample(_samplers[SamplerType::LinearClamp], uv - dir * (0.5)).rgb +
            _image.Sample(_samplers[SamplerType::LinearClamp], uv + dir * (0.5)).rgb);


    float lumaMin = min(lumaC, min(min(lumaNE, lumaNW), min(lumaSE, lumaSW)));
    float lumaMax = max(lumaC, max(max(lumaNE, lumaNW), max(lumaSE, lumaSW)));

	float lumaB = dot(sampleA, cLUMA);

	return ((lumaB < lumaMin) || (lumaB > lumaMax)) ? float4(sampleA , 1) : float4(sampleB , 1);

}
