#pragma once

#include <vulkan/vulkan.hpp>

#include "../BuildContext.hpp"

enum class AttachmentOp {
	ClearWrite,
	ReadWrite,
	Read,
};

struct RenderPass {
	static void Begin(Task::BuildContext& context, AttachmentOp color, AttachmentOp depth, vk::Rect2D viewport);
	static void Begin(Task::BuildContext& context, AttachmentOp color, AttachmentOp depth);

	static void QuadDraw(Task::BuildContext& context, MaterialIndex materialIndex);
	static void LoadIndirect(
		vk::CommandBuffer& commandBuffer,
		const std::vector<uint32_t>& primitivesIndices,
		const std::vector<Primitive>& primitives,
		Buffer& indirectBufferLocal,
		Buffer& indirectBufferDevice,
		Buffer& primitiveMapLocal,
		Buffer& primitiveMapDevice
	);
	static void IndirectDraw(
		Task::BuildContext& context,
		MaterialIndex material,
		const std::vector<uint32_t>& primitives,
		uint32_t pushConstant,
		std::size_t indirectSlot
	);

	static void End(vk::CommandBuffer& commandBuffer) { commandBuffer.endRendering(); }
};
