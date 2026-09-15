#pragma once

#include <vector>

#include "Camera.hpp"
#include "Light.hpp"
#include "Primitive.hpp"
#include "resources/ResourceManager.hpp"

struct Scene {
	enum MaterialHintBits : uint8_t {
		None = 0,
		Opaque = 1 << 0,
		AlphaMask = 1 << 1,
		ShadowCasting = 1 << 2,
	};
	using MaterialHint = uint8_t;
	struct PrimitiveBound {
		glm::vec3 position;
		float size = 0.0;
	};

	std::vector<Primitive> primitives;
	std::vector<MaterialHint> materialHints;
	std::vector<PrimitiveBound> primitiveBounds;
	float size = 0.0f;

	Camera camera;
	Light light;

	ResourceManager::AllocationIndex allocation;
};
