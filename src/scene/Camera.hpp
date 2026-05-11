#pragma once

#include <array>
#include <glm/glm.hpp>

class Camera {
public:
	struct Fov {
		float horizontal;
		float vertical;
	};

private:
	glm::vec3 m_position = glm::vec3(0, 0, 150);
	float m_pitch = 0;
	float m_yaw = 0;
	Fov m_fov;

	std::array<float, 4> m_frustumPlanesDistances { 0.1, 125, 700, 2500 };

	glm::ivec2 m_resolution = glm::ivec2(1280, 720);

	std::array<glm::vec3, 4> m_frustumEdgeDirections;
	std::array<glm::vec3, 4> m_baseFrustumPlanes;

	void updateFrustum();

public:
	void rotate(glm::vec2 rotation);
	void translate(glm::vec3 deltaPos) {
		m_position += getOrientation() * deltaPos;
	}
	void setResolution(glm::ivec2 resolution);
	glm::ivec2 getResolution() const { return m_resolution; }

	void setFov(float fov) {
		m_fov = {
			.horizontal = fov * m_resolution.x / m_resolution.y,
			.vertical = fov,
		};
	}

	glm::mat3 getOrientation() const;
	glm::vec3 getPosition() const { return m_position; };

	glm::mat4 getViewMatrix() const;
	glm::mat4 getProjectionMatrix() const;

	Fov getFov() const { return m_fov; };

	std::array<float, 4> getCascadeDistances() const {
		return m_frustumPlanesDistances;
	}
	std::array<glm::vec4, 6> getFrustumPlanes() const;

	using FrustumPoints = std::array<std::array<glm::vec4, 4>, 4>;
	FrustumPoints getFrustumPoints(const Camera& camera);
};
