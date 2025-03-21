#pragma once

#include <vector>

#include "Camera.hpp"
#include "Primitive.hpp"
#include "material/MaterialManager.hpp"
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

class Scene {
private:
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texcoord;
	};
	Buffer m_vertexBuffer;
	Buffer m_indexBuffer;
	std::vector<Primitive> m_primitives;
	Camera camera;

public:
	static Scene loadMesh(
		const std::filesystem::path& file,
		ResourceManager& resourceManager,
		MaterialManager& materialManager
	);

	const std::vector<Primitive>& getPrimitives() const { return m_primitives; }
	inline Buffer getIndexBuffer() { return m_indexBuffer; }
	inline Buffer getVertexBuffer() { return m_vertexBuffer; }

	inline Camera& getCamera() { return camera; }
};