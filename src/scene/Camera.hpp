#pragma once

#include <array>
#include <glm/glm.hpp>

class Camera {
public:
	enum class Mode {
		Orbital,
		FreeLook
	};
	using FrustumPoints = std::array<std::array<glm::vec4, 4>, 4>;

private:
	glm::vec3 m_position = glm::vec3(0, 0, 150);
	float m_pitch = 0;
	float m_yaw = 0;
	float m_vfov = 70;

	std::array<float, 4> m_frustumPlanes { 0.1, 125, 700, 2500 };
	glm::ivec2 m_resolution = glm::ivec2(1280, 720);

public:
	void rotate(glm::vec2 rotation);
	void translate(glm::vec3 deltaPos) { m_position += deltaPos; }
	void setResolution(glm::ivec2 resolution) { m_resolution = resolution; }

	glm::mat3 getOrientation() const;

	glm::mat4 getViewMatrix() const;
	glm::mat4 getProjectionMatrix() const;
	glm::vec2 getFov() const;

	FrustumPoints getFrustumPoints() const;
};