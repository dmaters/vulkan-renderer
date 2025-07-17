#pragma once

#include <assimp/mesh.h>
#include <assimp/scene.h>

#include <vector>

#include "Camera.hpp"
#include "Light.hpp"
#include "Primitive.hpp"

class Scene {
private:
	friend class SceneLoader;
	std::vector<Primitive> m_primitives;
	Camera camera;
	std::vector<Light> m_lights;

public:
	const std::vector<Primitive>& getPrimitives() const { return m_primitives; }
	const std::vector<Light> getLights() const { return m_lights; }

	Camera& getCamera() { return camera; }

	void addLight(Light light) { m_lights.push_back(light); }
	void addPrimitive(Primitive primitive) {
		m_primitives.push_back(primitive);
	}
};