#pragma once

#include "TaskContext.hpp"

void DrawPassIndirect(
	TaskContext& context,
	MaterialIndex material,
	const std::vector<uint32_t>& primitives
);
