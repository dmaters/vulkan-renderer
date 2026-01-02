#pragma once

#include <vulkan/vulkan.hpp>

#include "Task.hpp"
#include "material/MaterialManager.hpp"

enum class AttachmentOp {
	ClearWrite,
	ReadWrite,
	Read,
};

void RenderPass(
	TaskContext& context,
	MaterialIndex materialIndex,
	AttachmentOp color,
	AttachmentOp depth
);
