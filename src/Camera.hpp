#pragma once

#include <glm/glm.hpp>

class Camera {
public:
	enum class Mode {
		Orbital,
		FreeLook
	};

private:
	glm::vec3 m_position = glm::vec3(0, 0, -15);
	float m_pitch = 0;
	float m_yaw = 0;

public:
	void rotate(glm::vec2 rotation);

	glm::mat4 getViewVector();
};