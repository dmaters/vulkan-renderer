#pragma once

#include <vector>

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

	std::vector<Primitive> primitives;
	std::vector<MaterialHint> materialHint;

	struct PrimitiveBound {
		glm::vec3 position;
		float size;
	};

	std::vector<PrimitiveBound> primitiveBounds;
	float size;
};
