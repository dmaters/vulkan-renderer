#include "bindings/lighting.slang"
#include "bindings/geometry.slang"

float4  main(VertexInput input){
	return mul(LIGHT_DIRECT_PROJ,
	              mul(LIGHT_DATA[LIGHT_DIRECT_INDEX].trasformation,
	              float4(input.position, 1.0)));
}