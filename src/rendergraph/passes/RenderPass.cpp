#include "RenderPass.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "../BuildContext.hpp"
#include "../Task.hpp"
#include "material/MaterialManager.hpp"
#include "material/Pipeline.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

std::pair<vk::AttachmentLoadOp, vk::AttachmentStoreOp> getAttachmentOps(AttachmentOp operation) {
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

void RenderPass::Begin(Task::BuildContext& context, AttachmentOp color, AttachmentOp depth, vk::Rect2D viewport) {
	std::vector<vk::RenderingAttachmentInfo> attachments;

	std::vector<vk::RenderingAttachmentInfo> colorAttachments;
	std::optional<vk::RenderingAttachmentInfo> depthAttachment;
	bool hasStencil = false;

	for (auto [index, usage] : context.outputs) {
		if (usage != ResourceUsage::Type::ColorAttachmentWrite && usage != ResourceUsage::Type::DepthStencilWrite &&
			usage != ResourceUsage::Type::DepthStencilRead)
			continue;

		Image& attachment = context.resourceManager.getImage(context.images.at(index));

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
		.pDepthAttachment = depthAttachment.has_value() ? &depthAttachment.value() : nullptr,
		.pStencilAttachment = hasStencil ? &depthAttachment.value() : nullptr,

	};
	renderingInfo.renderArea = viewport, context.commandBuffer.beginRendering(renderingInfo);
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

	auto& buffers = context.resourceManager.getBuffers(context.scene.allocation);
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
	context.commandBuffer.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);
}

void RenderPass::Begin(Task::BuildContext& context, AttachmentOp color, AttachmentOp depth) {
	uint32_t width, height;
	for (auto [index, usage] : context.outputs) {
		if (usage != ResourceUsage::Type::ColorAttachmentWrite && usage != ResourceUsage::Type::DepthStencilWrite &&
			usage != ResourceUsage::Type::DepthStencilRead)
			continue;

		Image& attachment = context.resourceManager.getImage(context.images.at(index));

		width = attachment.size.width;
		height = attachment.size.height;
		break;
	}

	RenderPass::Begin(
		context,
		color,
		depth,
		{
			{ 0,	 0	   },
			{ width, height },
	}
	);
}

void RenderPass::QuadDraw(Task::BuildContext& context, MaterialIndex materialIndex) {
	const Pipeline& material = context.materialManager.getPipeline(materialIndex);

	context.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, material.pipeline);

	context.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics, material.pipelineLayout, 1, { context.materialManager.getTextureSet() }, {}
	);

	Task::BuildContext::Descriptors descriptors = context.getDescriptors();

	if (descriptors.descriptors.size() > 0) {
		context.commandBuffer.pushDescriptorSet(
			vk::PipelineBindPoint::eGraphics, material.pipelineLayout, 0, descriptors.descriptors
		);
	} else
		context.commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics, material.pipelineLayout, 0, { context.materialManager.getEmptySet() }, {}
		);
	context.commandBuffer.draw(3, 1, 0, 0);
}

void RenderPass::LoadIndirect(
	vk::CommandBuffer& commandBuffer,
	const std::vector<uint32_t>& primitivesIndices,
	const std::vector<Primitive>& primitives,
	Buffer& indirectBufferLocal,
	Buffer& indirectBufferDevice,
	Buffer& primitiveMapLocal,
	Buffer& primitiveMapDevice
) {
	auto indirectBufferValues = (vk::DrawIndexedIndirectCommand*)indirectBufferLocal.data;

	auto primitiveBufferValues = (uint32_t*)primitiveMapLocal.data;

	for (int i = 0; i < primitivesIndices.size(); i++) {
		uint32_t primitiveIndex = primitivesIndices[i];
		const Primitive& primitive = primitives[primitiveIndex];

		indirectBufferValues[i] = vk::DrawIndexedIndirectCommand {
			.indexCount = primitive.indexCount,
			.instanceCount = 1,
			.firstIndex = primitive.baseIndex,
			.vertexOffset = static_cast<int32_t>(primitive.baseVertex),
			.firstInstance = 0,
		};

		primitiveBufferValues[i] = primitiveIndex;
	}

	vk::BufferCopy2 indirectCopy {
		.size = indirectBufferLocal.size,
	};
	commandBuffer.copyBuffer2(
		{
			.srcBuffer = indirectBufferLocal.buffer,
			.dstBuffer = indirectBufferDevice.buffer,
			.regionCount = 1,
			.pRegions = &indirectCopy,
		}
	);
	vk::BufferCopy2 primitiveMapCopy {
		.size = primitiveMapDevice.size,
	};
	commandBuffer.copyBuffer2(
		{
			.srcBuffer = primitiveMapLocal.buffer,
			.dstBuffer = primitiveMapDevice.buffer,
			.regionCount = 1,
			.pRegions = &primitiveMapCopy,
		}
	);

	std::array<vk::BufferMemoryBarrier2, 2> barriers = {
		vk::BufferMemoryBarrier2 {
								  .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
								  .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
								  .dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect,
								  .dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
								  .buffer = indirectBufferDevice.buffer,
								  .size = indirectBufferDevice.size,
								  },
		vk::BufferMemoryBarrier2 {
								  .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
								  .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
								  .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
								  .dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead,
								  .buffer = primitiveMapDevice.buffer,
								  .size = primitiveMapDevice.size,
								  },
	};

	commandBuffer.pipelineBarrier2(
		{
			.bufferMemoryBarrierCount = 2,
			.pBufferMemoryBarriers = barriers.data(),
		}
	);
}

void RenderPass::IndirectDraw(
	Task::BuildContext& context,
	MaterialIndex material,
	const std::vector<uint32_t>& primitives,
	uint32_t pushConstant,
	std::size_t indirectSlot
) {
	Buffer& indirectBufferDevice = context.getInput<Buffer&>(indirectSlot);

	const Pipeline& pipeline = context.materialManager.getPipeline(material);

	context.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.pipeline);

	context.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 1, { context.materialManager.getTextureSet() }, {}
	);

	Task::BuildContext::Descriptors descriptors = context.getDescriptors();

	if (descriptors.descriptors.size() > 0) {
		context.commandBuffer.pushDescriptorSet(
			vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, descriptors.descriptors
		);
	} else
		context.commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, { context.materialManager.getEmptySet() }, {}
		);

	context.commandBuffer.pushConstants(
		pipeline.pipelineLayout,
		vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		0,
		sizeof(uint32_t),
		&pushConstant
	);

	context.commandBuffer.drawIndexedIndirect(
		indirectBufferDevice.buffer, 0, primitives.size(), sizeof(vk::DrawIndexedIndirectCommand)
	);
}
