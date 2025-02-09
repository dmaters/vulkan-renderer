#pragma once

#include <vector>

#include "Camera.hpp"
#include "Drawable.hpp"
#include "material/MaterialManager.hpp"
#include "resources/MemoryAllocator.hpp"

class Scene {
private:
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texcoord;
	};

	std::vector<Drawable> m_drawables;
	Camera camera;

public:
	static Scene loadMesh(
		const std::filesystem::path& file,
		MemoryAllocator& allocator,
		MaterialManager& materialManager
	);

	const std::vector<Drawable>& getDrawables() const { return m_drawables; }

	inline Camera& getCamera() { return camera; }
};