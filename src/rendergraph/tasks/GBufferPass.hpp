#pragma once
#include "../Task.hpp"
#include "material/MaterialManager.hpp"

struct GBUfferPass {
	TaskIndex sceneUpdatePass;
	MaterialIndex material;
	rendergraph::ResourceIndex pbrMaterialData;
	rendergraph::ResourceIndex pbrMaterialInstances;

	std::array<rendergraph::ResourceIndex, 3> _indirectBuffer;
	std::array<rendergraph::ResourceIndex, 3> _primitiveMap;

	std::array<rendergraph::ResourceIndex, 3> _alphaIndirectBuffer;
	std::array<rendergraph::ResourceIndex, 3> _alphaPrimitiveMap;

	static void Setup(Task::SetupContext&);
	static void Build(Task::BuildContext&);
};
