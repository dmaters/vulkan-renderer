#include "bindings/lighting.slang"
#include "bindings/geometry.slang"
#include "comp/utils.slang"
#include "bindings/environment.slang"
#include "comp/shadow.slang"


[shader("vertex")]
float4  main(VertexInput input) : SV_Position {
	return 	mul(mul(mul(mul(Shadow::shadowProjectionMatrix(),uLightData.lights[uLightData.directIndex].view ),uCamera.view ),uPrimitive.transformation), float4(input.position, 1.0));
}