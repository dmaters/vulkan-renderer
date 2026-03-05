#pragma once

#include <vulkan/vulkan.hpp>

#include "TaskContext.hpp"

enum class AttachmentOp {
	ClearWrite,
	ReadWrite,
	Read,
};

void RenderPassBegin(
	TaskContext& context,
	AttachmentOp color,
	AttachmentOp depth,
	vk::Rect2D viewport
);

void RenderPassBegin(
	TaskContext& context, AttachmentOp color, AttachmentOp depth
);

constexpr void RenderPassEnd(TaskContext& context) {
	context.commandBuffer.endRendering();
}
