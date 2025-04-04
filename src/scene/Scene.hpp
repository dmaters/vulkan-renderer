#pragma once

#include <assimp/mesh.h>
#include <assimp/scene.h>

#include <vector>

#include "Camera.hpp"
#include "Primitive.hpp"

class Scene {
private:
	friend class SceneLoader;
	std::vector<Primitive> m_primitives;
	Camera camera;

public:
	const std::vector<Primitive>& getPrimitives() const { return m_primitives; }

	inline Camera& getCamera() { return camera; }
};