[vk_binding(0)] Texture2D _base;
[vk_binding(1)] RWTexture2D _result[];

[vk_push_constant] uint _mipLevel;

[numthreads(1,1,1)]
[shader("compute")]
void main(uint3 dispatchIndex : SV_GroupID){

        uint2 texelPos =  dispatchIndex.xy * 2;


        if(_mipLevel == 1)
        {
            float4 s1 = _base.Load(uint3(texelPos.x, texelPos.y, 0));
            float4 s2 = _base.Load(uint3(texelPos.x + 1, texelPos.y, 0));
            float4 s3 = _base.Load(uint3(texelPos.x, texelPos.y + 1, 0));
            float4 s4 = _base.Load(uint3(texelPos.x + 1, texelPos.y + 1, 0));

            _result[1].Store(uint2(texelPos.x, texelPos.y),s1);
            _result[1].Store(uint2(texelPos.x + 1, texelPos.y),s2);
            _result[1].Store(uint2(texelPos.x, texelPos.y + 1),s3);
            _result[1].Store(uint2(texelPos.x + 1, texelPos.y + 1),s4);
        }

        float4 s1 =  _result[_mipLevel].Load(uint2(texelPos.x, texelPos.y));
        float4 s2 =  _result[_mipLevel].Load(uint2(texelPos.x + 1, texelPos.y));
        float4 s3 =  _result[_mipLevel].Load(uint2(texelPos.x, texelPos.y + 1));
        float4 s4 =  _result[_mipLevel].Load(uint2(texelPos.x + 1, texelPos.y + 1));

        float4 sampledValue = min(min(s1,s2),min(s3,s4));

        _result[_mipLevel + 1].Store(dispatchIndex.xy, sampledValue);

}
