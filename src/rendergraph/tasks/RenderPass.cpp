#include "RenderPass.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "rendergraph/RenderGraph.hpp"

void RenderPass::setup(
	std::vector<ImageDependencyInfo>& requiredImages,
	std::vector<BufferDependencyInfo>& requiredBuffers
) {
	requiredImages.push_back({
		.name = "vertex_buffer",
		.usage = {
				  .type = ResourceUsage::Type::READ,
				  .access = vk::AccessFlagBits2::eVertexAttributeRead,
				  .stage = vk::PipelineStageFlagBits2::eVertexAttributeInput,
				  }
    });

	requiredBuffers.push_back({
		.name = "index_buffer",
		.usage = {
				  .type = ResourceUsage::Type::READ,
				  .access = vk::AccessFlagBits2::eIndexRead,
				  .stage = vk::PipelineStageFlagBits2::eIndexInput,
				  }
    });
}

void RenderPass::execute(
	vk::CommandBuffer& commandBuffer, const Resources& resources
) {
	assert(m_attachments.color.has_value() || m_attachments.depth.has_value());

	vk::RenderingInfoKHR renderingInfo {
		.renderArea = vk::Rect2D({ 0, 0 }, { 800, 600 }),
		.layerCount = 1,
		.viewMask = 0,
		.pDepthAttachment = nullptr,

	};
	vk::RenderingAttachmentInfo colorAttachment;
	uint32_t width, height;
	if (m_attachments.color.has_value()) {
		Image& color = resources.resourceManager.getNamedImage(
			m_attachments.color.value().name
		);

		colorAttachment= {
			.imageView = color.accesses[resources.currentFrame].view,
			.imageLayout = color.accesses[resources.currentFrame].layout,
	
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = { .color =
								vk::ClearColorValue {
									.float32 =
										std::array<float, 4> {
											0.f, 0.f, 0.f, 0.f }, }, },
		};

		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;
		width = color.size.width;
		height = color.size.height;
	}

	vk::RenderingAttachmentInfo depthAttachment;
	if (m_attachments.depth.has_value()) {
		Image& depth = resources.resourceManager.getNamedImage("main_depth");

		depthAttachment = {
			.imageView = depth.accesses[resources.currentFrame].view,
			.imageLayout = depth.accesses[resources.currentFrame].layout,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = { .depthStencil = { 1, 1 } },
		};

		renderingInfo.pDepthAttachment = &depthAttachment;
		width = depth.size.width;
		height = depth.size.height;
	}

	commandBuffer.beginRendering(renderingInfo);
	commandBuffer.setScissor(
		0,
		{
			vk::Rect2D { { 0, 0 }, { width, height } }
    }
	);

	commandBuffer.setViewport(
		0,
		{
			vk::Viewport { 0, 0, (float)width, (float)height, 0, 1 }
    }
	);
	commandBuffer.bindPipeline(
		vk::PipelineBindPoint::eGraphics, m_material->pipeline.pipeline
	);

	Buffer& vertexBuffer =
		resources.resourceManager.getNamedBuffer("vertex_buffer");
	Buffer& indexBuffer =
		resources.resourceManager.getNamedBuffer("index_buffer");

	commandBuffer.bindVertexBuffers(0, { vertexBuffer.buffer }, { 0 });
	commandBuffer.bindIndexBuffer(
		indexBuffer.buffer, 0, vk::IndexType::eUint32
	);

	commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		m_material->pipeline.pipelineLayout,
		0,
		{ m_material->globalSet.set },
		{}
	);
}