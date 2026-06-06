#include "RenderPass.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Task.hpp"
#include "TaskContext.hpp"
#include "resources/ResourceManager.hpp"

std::pair<vk::AttachmentLoadOp, vk::AttachmentStoreOp> getAttachmentOps(
	AttachmentOp operation
) {
	switch (operation) {
		case AttachmentOp::ClearWrite:
			return {
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
			};
		case AttachmentOp::ReadWrite:
			return {
				vk::AttachmentLoadOp::eLoad,
				vk::AttachmentStoreOp::eStore,
			};
		case AttachmentOp::Read:
			return {
				vk::AttachmentLoadOp::eLoad,
				vk::AttachmentStoreOp::eNone,
			};
	}
}

void RenderPassBegin(
	TaskContext& context,
	AttachmentOp color,
	AttachmentOp depth,
	vk::Rect2D viewport
) {
	std::vector<vk::RenderingAttachmentInfo> attachments;

	std::vector<vk::RenderingAttachmentInfo> colorAttachments;
	std::optional<vk::RenderingAttachmentInfo> depthAttachment;
	bool hasStencil = false;

	for (auto [index, usage] : context.outputs) {
		if (usage != ResourceUsage::Type::ColorAttachmentWrite &&
		    usage != ResourceUsage::Type::DepthStencilWrite &&
		    usage != ResourceUsage::Type::DepthStencilRead)
			continue;

		Image& attachment =
			context.resourceManager.getImage(context.images.at(index));

		bool isStencil = attachment.format == vk::Format::eD24UnormS8Uint;
		if (isStencil) hasStencil = true;

		bool isDepth = attachment.format == vk::Format::eD16Unorm || isStencil;
		if (isDepth) {
			auto [loadOp, storeOp] = getAttachmentOps(depth);

			depthAttachment = vk::RenderingAttachmentInfo{
				.imageView = attachment.view,
				.imageLayout =
					isStencil ? vk::ImageLayout::eDepthStencilAttachmentOptimal : vk::ImageLayout::eDepthAttachmentOptimal,
				.loadOp = loadOp,
				.storeOp = storeOp,
				.clearValue = { .depthStencil = { .depth = 1, .stencil = 0 }, },
			};
		} else {
			auto [loadOp, storeOp] = getAttachmentOps(color);

			colorAttachments.push_back({
			.imageView = attachment.view,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = loadOp,
			.storeOp = storeOp,
			.clearValue = { .color =
								vk::ClearColorValue {
									.float32 =
										std::array<float, 4> {
											0.f, 0.f, 0.f, 0.f }, }, },
			});
		}
	}

	assert(colorAttachments.size() > 0 || depthAttachment.has_value());

	vk::RenderingInfo renderingInfo {
		.layerCount = 1,
		.viewMask = 0,
		.colorAttachmentCount = (uint32_t)colorAttachments.size(),
		.pColorAttachments = colorAttachments.data(),
		.pDepthAttachment =
			depthAttachment.has_value() ? &depthAttachment.value() : nullptr,
		.pStencilAttachment = hasStencil ? &depthAttachment.value() : nullptr,

	};
	renderingInfo.renderArea = viewport,
	context.commandBuffer.beginRendering(renderingInfo);
	context.commandBuffer.setScissor(0, viewport);

	context.commandBuffer.setViewport(
		0,
		{
			vk::Viewport {
						  (float)viewport.offset.x,
						  (float)viewport.offset.y,
						  (float)viewport.extent.width,
						  (float)viewport.extent.height,
						  0, 1,
						  }
    }
	);

	auto& buffers =
		context.resourceManager.getBuffers(context.scene.allocation);
	Buffer& positionBuffer = context.resourceManager.getBuffer(buffers[0]);
	Buffer& attributeBuffer = context.resourceManager.getBuffer(buffers[1]);
	Buffer& indexBuffer = context.resourceManager.getBuffer(buffers[2]);
	Buffer& instanceBuffer = context.resourceManager.getBuffer(buffers[3]);

	context.commandBuffer.bindVertexBuffers(
		0,
		{
			positionBuffer.buffer,
			attributeBuffer.buffer,
			instanceBuffer.buffer,
		},
		{ 0, 0, 0 }
	);
	context.commandBuffer.bindIndexBuffer(
		indexBuffer.buffer, 0, vk::IndexType::eUint32
	);
}

void RenderPassBegin(
	TaskContext& context, AttachmentOp color, AttachmentOp depth
) {
	uint32_t width, height;
	for (auto [index, usage] : context.outputs) {
		if (usage != ResourceUsage::Type::ColorAttachmentWrite &&
		    usage != ResourceUsage::Type::DepthStencilWrite &&
		    usage != ResourceUsage::Type::DepthStencilRead)
			continue;

		Image& attachment =
			context.resourceManager.getImage(context.images.at(index));

		width = attachment.size.width;
		height = attachment.size.height;
		break;
	}

	RenderPassBegin(
		context,
		color,
		depth,
		{
			{ 0,     0      },
			{ width, height },
    }
	);
}
