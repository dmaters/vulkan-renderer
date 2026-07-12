#include "FrustumCulling.hpp"

std::vector<uint32_t> FrustumCulling(
	const Scene& scene, const std::vector<uint32_t>& bucket, const std::array<glm::vec4, 6>& planes
) {
	std::vector<uint32_t> filtered;
	glm::vec3 cameraPos = scene.camera.getPosition();
	for (uint32_t index : bucket) {
		Scene::PrimitiveBound primitivePos = scene.primitiveBounds[index];

		bool outside = false;

		for (int i = 0; i < 6; i++) {
			float distanceToPlane = glm::dot(glm::vec3(planes[i]), primitivePos.position - cameraPos);

			if ((distanceToPlane - primitivePos.size - planes[i].w) < 0) continue;

			outside = true;
			break;
		}

		if (!outside) filtered.push_back(index);
	}

	return filtered;
}
