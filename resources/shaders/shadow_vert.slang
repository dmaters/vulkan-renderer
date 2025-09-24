#include "bindings/lighting.slang"
#include "bindings/geometry.slang"
#include "comp/utils.slang"
#include "bindings/environment.slang"
#include "comp/shadow.slang"

[vk::push_constant] cbuffer ShadowData {
	uint32_t cascade;
} uShadowData;

[shader("vertex")]
float4  main(VertexInput input) : SV_Position {
	return 	mul(mul(mul(Shadow::shadowProjectionMatrix(uShadowData.cascade),uLightData.lights[uLightData.directIndex].view ),input.transformation), float4(input.position, 1.0));
}