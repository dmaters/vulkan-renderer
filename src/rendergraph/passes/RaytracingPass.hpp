#pragma once
#include "material/MaterialManager.hpp"
#include "rendergraph/Task.hpp"
#include "resources/ResourceManager.hpp"

struct RaytracingPass {
	static ResourceManager::BufferDescription GetSBTDescription();
	static void TraceRays(
		Task::BuildContext& context,
		MaterialIndex materialIndex,
		rendergraph::ResourceIndex sbt,
		glm::uvec3 dispatchSize
	);
};
