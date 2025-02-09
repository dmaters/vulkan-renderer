#version 450

layout(location = 0) out vec4 outColor;
layout(location = 3) in vec3 normal;
layout(location = 4) in vec2 textCoords;
layout(set = 1, binding = 0) uniform sampler2D albedo;
void main() { outColor = vec4(texture(albedo, textCoords).rgb, 1.0); }