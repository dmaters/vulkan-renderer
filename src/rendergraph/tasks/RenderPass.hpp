#pragma once

#include <string_view>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Task.hpp"
#include "material/MaterialManager.hpp"

void RenderPass(
	TaskContext& context,
	MaterialIndex materialIndex,
	const std::vector<Primitive>& primitives
);