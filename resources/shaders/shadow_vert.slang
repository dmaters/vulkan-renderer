#include "bindings/lighting.slang"
#include "bindings/geometry.slang"

#include "bindings/environment.slang"



[shader("vertex")]
float4  main(VertexInput input) : SV_Position {
	return 	mul(mul(mul(directLightProjectionMatrix(uEnvironment.scene_size),uLightData.lights[uLightData.directIndex].trasformation ),uPrimitive.transformation), float4(input.position, 1.0));
}