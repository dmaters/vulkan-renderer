#pragma once

#include <glm/glm.hpp>

struct Camera {
	glm::vec3 position = glm::vec3(0, 0, 150);
	float pitch = 0;
	float yaw = 0;

	struct Fov {
		float horizontal;
		float vertical;
	};
	glm::vec2 fov;

	struct ShaderObject {
		glm::mat4 view;
		glm::mat4 projection;
		glm::mat4 invView;
		glm::mat4 invProj;
		glm::vec4 position;
		glm::vec4 direction;
		float nearPlane;
		float farPlane;
	};

	float getFrustumSize(float sceneSize) const;
	std::array<glm::vec3, 4> getFrustumDirections() const;
	std::array<glm::vec4, 6> getFrustumPlanes(float sceneSize) const;

	glm::mat3 getOrientation() const;
	glm::mat4 getViewMatrix() const;
	glm::mat4 getProjectionMatrix(glm::ivec2 resolution, float sceneSize) const;
	ShaderObject getShaderObject();
};
