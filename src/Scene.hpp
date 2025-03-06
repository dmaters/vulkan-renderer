#pragma once

#include <vector>

#include "Camera.hpp"
#include "Primitive.hpp"
#include "material/MaterialManager.hpp"
#include "resources/MemoryAllocator.hpp"

class Scene {
private:
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texcoord;
	};

	std::vector<Primitive> m_primitives;
	Camera camera;

public:
	static Scene loadMesh(
		const std::filesystem::path& file,
		MemoryAllocator& allocator,
		MaterialManager& materialManager
	);

	const std::vector<Primitive>& getPrimitives() const { return m_primitives; }

	inline Camera& getCamera() { return camera; }
};