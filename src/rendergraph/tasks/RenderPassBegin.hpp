#pragma once

#include <vulkan/vulkan.hpp>

#include "TaskContext.hpp"

enum class AttachmentOp {
	ClearWrite,
	ReadWrite,
	Read,
};

void RenderPassBegin(
	TaskContext& context, AttachmentOp color, AttachmentOp depth
);
