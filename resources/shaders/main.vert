#version 450

layout(location = 0) out vec3 fragColor;

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 3) out vec3 fragNormal;
layout(location = 4) out vec2 fragTexCoord;

layout(set = 0, binding = 0) uniform ViewProjectionData {
	mat4 view;
	mat4 projection;
};

layout(push_constant) uniform PushConstants { mat4 model; };

void main() {
	gl_Position = projection * view * model * vec4(inPosition, 1.0);

	fragNormal = inNormal;
	fragTexCoord = inTexCoord;
}