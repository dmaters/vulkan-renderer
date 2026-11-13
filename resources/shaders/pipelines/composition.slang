import tech.tonemapping;

[vk_binding(0)] Texture2D _base;
[vk_binding(1)] WTexture2D<float4> _output;

static const float EXPOSURE_BIAS = 10;

void main(uint3 dispatchThreadID : SV_GroupID) {
	uint2 coord = dispatchThreadID.xy;

	float3 hdrColor = _base.Load(int3(coord, 0)).rgb;
	float3 mapped = tonemap_GT7(hdrColor * EXPOSURE_BIAS);
	_output.Store(coord, float4(mapped, 1.0));
}
