#version 450

layout(location = 0) in vec4 fragPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 textCoords;

layout(location = 0) out vec4 outColor;
struct Light {
	mat4 trasformation;
	vec4 color_intensity;
};

layout(set = 0, binding = 1) uniform LightData {
	uint count;
	Light[256] lights;
}
lights;

layout(set = 1, binding = 0) uniform sampler2D albedo;
void main() {
	outColor = vec4(0, 0, 0, 1);
	for (int i = 0; i < lights.count; i++) {
		Light light = lights.lights[i];

		outColor += vec4(
			texture(albedo, textCoords).rgb * light.color_intensity.xyz *
				dot(normal, light.trasformation[3].xyz) *
				light.color_intensity.w,
			1.0
		);
	}
}