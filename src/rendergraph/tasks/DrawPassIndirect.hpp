#pragma once

#include "TaskContext.hpp"

void DrawPassIndirect(
	TaskContext& context,
	MaterialIndex material,
	const std::vector<uint32_t>& primitives,
	uint32_t indirectBufferIndex,
	uint32_t primitiveBufferIndex,
	uint32_t pushConstant = 0
);
