#pragma once

#include <vulkan/vulkan.hpp>

#include "../Task.hpp"
#include "material/MaterialManager.hpp"

void ComputePass(
	Task::BuildContext& context,
	MaterialIndex materialIndex,
	glm::uvec3 dispatchSize
);
