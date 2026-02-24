#pragma once

#include "TaskContext.hpp"

constexpr void RenderPassEnd(TaskContext& context) {
	context.commandBuffer.endRendering();
}
