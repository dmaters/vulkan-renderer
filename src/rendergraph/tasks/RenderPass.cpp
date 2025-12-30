#include "RenderPass.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Task.hpp"
#include "TaskContext.hpp"
#include "material/MaterialManager.hpp"
#include "material/Pipeline.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

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

void setupAttachments(
	TaskContext& context, AttachmentOp colorOp, AttachmentOp depthOp
) {
	std::vector<vk::RenderingAttachmentInfo> attachments;

	std::vector<vk::RenderingAttachmentInfo> colorAttachments;
	std::optional<vk::RenderingAttachmentInfo> depthAttachment;
	bool hasStencil = false;

	uint32_t width = 0, height = 0;

	for (auto [index, _] : context.outputs) {
		Image& attachment =
			context.resourceManager.getImage(context.images.at(index));

		bool isStencil = attachment.format == vk::Format::eD24UnormS8Uint;
		if (isStencil) hasStencil = true;

		bool isDepth = attachment.format == vk::Format::eD16Unorm || isStencil;
		if (isDepth) {
			auto [loadOp, storeOp] = getAttachmentOps(depthOp);

			depthAttachment = vk::RenderingAttachmentInfo{
				.imageView = attachment.view,
				.imageLayout =
					isStencil ? vk::ImageLayout::eDepthStencilAttachmentOptimal : vk::ImageLayout::eDepthAttachmentOptimal,
				.loadOp = loadOp,
				.storeOp = storeOp,
				.clearValue = { .depthStencil = { .depth = 1, .stencil = 0 }, },
			};

			width = attachment.size.width;
			height = attachment.size.height;

		} else {
			auto [loadOp, storeOp] = getAttachmentOps(colorOp);

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

		width = attachment.size.width;
		height = attachment.size.height;
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
	renderingInfo.renderArea = vk::Rect2D({ 0, 0 }, { width, height }),
	context.commandBuffer.beginRendering(renderingInfo);
	context.commandBuffer.setScissor(
		0,
		{
			vk::Rect2D { { 0, 0 }, { width, height } }
    }
	);

	context.commandBuffer.setViewport(
		0,
		{
			vk::Viewport { 0, 0, (float)width, (float)height, 0, 1 }
    }
	);

	Buffer& positionBuffer =
		context.resourceManager.getNamedBuffer("vertex_positions_buffer");
	Buffer& attributeBuffer =
		context.resourceManager.getNamedBuffer("vertex_attributes_buffer");
	Buffer& instanceBuffer =
		context.resourceManager.getNamedBuffer("instance_buffer");
	Buffer& indexBuffer =
		context.resourceManager.getNamedBuffer("index_buffer");

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

void RenderPass(
	TaskContext& context,
	MaterialIndex materialIndex,
	AttachmentOp colorOp,
	AttachmentOp depthOp
) {
	setupAttachments(context, colorOp, depthOp);

	const Pipeline& material =
		context.materialManager.getPipeline(materialIndex);

	context.commandBuffer.bindPipeline(
		vk::PipelineBindPoint::eGraphics, material.pipeline
	);

	context.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		material.pipelineLayout,
		1,
		{ context.materialManager.getTextureSet() },
		{}
	);

	TaskContext::Descriptors descriptors = context.getDescriptors(true);

	if (descriptors.descriptors.size() > 0) {
		context.commandBuffer.pushDescriptorSet(
			vk::PipelineBindPoint::eGraphics,
			material.pipelineLayout,
			0,
			descriptors.descriptors
		);
	} else
		context.commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			material.pipelineLayout,
			0,
			{ context.materialManager.getEmptySet() },
			{}
		);

	for (const uint32_t primitiveIndex :
	     context.scene.buckets.at(materialIndex)) {
		const Primitive& primitive = context.scene.primitives[primitiveIndex];

		context.commandBuffer.pushConstants(
			material.pipelineLayout,
			vk::ShaderStageFlagBits::eVertex |
				vk::ShaderStageFlagBits::eFragment,
			0,
			sizeof(uint32_t),
			&primitiveIndex  // TODO: better indexing for material instances
		);

		context.commandBuffer.drawIndexed(
			primitive.indexCount,
			primitive.instanceCount,
			primitive.baseIndex,
			primitive.baseVertex,
			primitive.baseInstance
		);
	}

	context.commandBuffer.endRendering();
}
