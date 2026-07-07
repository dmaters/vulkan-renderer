#pragma once

#include <array>

#include "../ResourceIndex.hpp"
#include "../Task.hpp"
#include "material/MaterialManager.hpp"

struct ShadowPass {
	std::array<rendergraph::ResourceIndex, 3> _indirectBuffer;
	std::array<rendergraph::ResourceIndex, 3> _primitiveMap;

	TaskIndex sceneUpdateTask;

	MaterialIndex material;

	static Task::Dependencies Setup(Task::SetupContext&);
	static void Build(Task::BuildContext&);
};
