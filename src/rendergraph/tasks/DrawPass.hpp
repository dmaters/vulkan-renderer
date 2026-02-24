#pragma once

#include "TaskContext.hpp"

void DrawPass(
	TaskContext& context,
	MaterialIndex material,
	const std::vector<uint32_t>& primitives
);
