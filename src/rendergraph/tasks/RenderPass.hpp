#pragma once

#include <vulkan/vulkan.hpp>

#include "../BuildContext.hpp"

enum class AttachmentOp {
	ClearWrite,
	ReadWrite,
	Read,
};

struct RenderPass {
	static void Begin(
		Task::BuildContext& context,
		AttachmentOp color,
		AttachmentOp depth,
		vk::Rect2D viewport
	);
	static void Begin(
		Task::BuildContext& context, AttachmentOp color, AttachmentOp depth
	);
	static void End(vk::CommandBuffer& commandBuffer) {
		commandBuffer.endRendering();
	}
};
