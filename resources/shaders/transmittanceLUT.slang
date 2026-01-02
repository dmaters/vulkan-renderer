
#include "comp/sky/sky.slang"

[vk_binding(0)] WTexture2D<float4> _transmittanceLUT;

void main(uint3 dispatch : SV_GroupID) {
	writeTransmittanceLUT(_transmittanceLUT, dispatch.xy);
}
