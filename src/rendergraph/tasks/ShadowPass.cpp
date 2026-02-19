#include "ShadowPass.hpp"

#include <vulkan/vulkan.hpp>

#include "TaskContext.hpp"
#include "resources/Buffer.hpp"

constexpr int32_t CASCADE_SIZE = 2048;

void ShadowPass(TaskContext& context, uint8_t cascade) {
	MaterialIndex materialIndex =
		context.materialManager.getMaterialIndex("shadowmap");
	const Pipeline material =
		context.materialManager.getPipeline(materialIndex);

	context.commandBuffer.bindPipeline(
		vk::PipelineBindPoint::eGraphics, material.pipeline
	);

	vk::DescriptorSet textureSet = context.materialManager.getTextureSet();

	context.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		material.pipelineLayout,
		1,
		{ textureSet },
		{}
	);
	ImageHandle handle = context.images[context.outputs[0].first];
	Image& image = context.resourceManager.getImage(handle);

	vk::RenderingAttachmentInfo attachment {

		.imageView = image.view,
		.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = { .depthStencil = { .depth = 1, .stencil = 0 }, },

	};

	vk::Rect2D renderArea = {
		{
         CASCADE_SIZE * cascade,
         0, },
		{ CASCADE_SIZE, CASCADE_SIZE }
	};

	context.commandBuffer.beginRendering(
		vk::RenderingInfo {
			.renderArea = renderArea,
			.layerCount = 1,
			.pDepthAttachment = &attachment,
		}
	);
	context.commandBuffer.setScissor(0, { renderArea });
	context.commandBuffer.setViewport(
		0,
		{
			{
             .x = (float)renderArea.offset.x,
             .width = CASCADE_SIZE,
             .height = CASCADE_SIZE,
             .maxDepth = 1.0f,
			 }
    }
	);
	uint32_t pcascade = cascade;
	context.commandBuffer.pushConstants(
		material.pipelineLayout,
		vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		0,
		sizeof(uint32_t),
		&pcascade
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

	auto descriptors = context.getDescriptors();
	context.commandBuffer.pushDescriptorSet(
		vk::PipelineBindPoint::eGraphics,
		material.pipelineLayout,
		0,
		descriptors.descriptors
	);

	auto outputSpan = context.getOutputSpan<vk::DrawIndexedIndirectCommand>(
		context.outputs.size() - 1, true
	);
	uint baseOffset = outputSpan.size() / 3 * cascade;

	const auto& bucket = context.scene.buckets.at(materialIndex);
	for (int i = 0; i < bucket.size(); i++) {
		PipelineIndex primitiveIndex = bucket[i];
		const Primitive& primitive = context.scene.primitives[primitiveIndex];

		outputSpan[baseOffset + i] = vk::DrawIndexedIndirectCommand {
			.indexCount = primitive.indexCount,
			.instanceCount = 1,
			.firstIndex = primitive.baseIndex,
			.vertexOffset = static_cast<int32_t>(primitive.baseVertex),
			.firstInstance = 0,

		};
	}

	Buffer& indirectBuffer =
		context.getInput<Buffer&>(context.inputs.size() - 1);

	if (context.currentFrame > 3)
		context.commandBuffer.drawIndexedIndirect(
			indirectBuffer.buffer,
			baseOffset * sizeof(vk::DrawIndexedIndirectCommand),
			bucket.size(),
			sizeof(vk::DrawIndexedIndirectCommand)
		);

	context.commandBuffer.endRendering();
}
