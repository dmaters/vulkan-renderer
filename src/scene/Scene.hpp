#pragma once

#include <unordered_map>
#include <vector>

#include "Camera.hpp"
#include "Light.hpp"
#include "Primitive.hpp"
#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"

struct Scene {
	Camera camera;
	std::vector<Light> lights;
	std::vector<Primitive> primitives;

	struct PrimitiveBound {
		glm::vec3 position;
		float size;
	};
	std::vector<PrimitiveBound> primitiveBounds;
	float size;

	ResourceManager::DeviceAllocationIndex allocation;
	std::unordered_map<MaterialIndex, std::vector<uint32_t>> buckets;
};
