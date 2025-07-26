#pragma once

#include <assimp/mesh.h>
#include <assimp/scene.h>

#include <vector>

#include "Camera.hpp"
#include "Light.hpp"
#include "Primitive.hpp"

struct Scene {
	Camera camera;
	std::vector<Light> lights;
	std::vector<Primitive> primitives;

	float size;
};