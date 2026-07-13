#pragma once
#include <glm/glm.hpp>
#include <vector>

#include "scene/Scene.hpp"

std::vector<uint32_t> FrustumCulling(
	const Scene& scene, const std::vector<uint32_t>& bucket, const std::array<glm::vec4, 6>& planes
);
