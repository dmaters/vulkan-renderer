#include "RenderPass.hpp"

#include <map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphResourceSolver.hpp"

void RenderPass::setup(RenderGraphResourceSolver& renderGraph) {
	renderGraph
		.registerDependency(RenderGraphResourceSolver::BufferDependencyInfo {
			.name = "vertex_buffer",
			.usage = {
					  .type = ResourceUsage::Type::READ,
					  .access = vk::AccessFlagBits2::eVertexAttributeRead,
					  .stage = vk::PipelineStageFlagBits2::eVertexAttributeInput,
					  }
    });

	renderGraph
		.registerDependency(RenderGraphResourceSolver::BufferDependencyInfo {
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
	vk::RenderingAttachmentInfo colorAttachment {
		.imageView =
			resources.transientImages
				.at(m_attachments.color.value())[resources.currentFrame]
				.view,
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal
	};
	vk::RenderingAttachmentInfo depthAttachment {
		.imageView =
			resources.transientImages
				.at(m_attachments.depth.value())[resources.currentFrame]
				.view,
		.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal
	};
	vk::RenderingInfoKHR renderingInfo {
		.renderArea = vk::Rect2D({ 0, 0 }, { 800, 600 }),
		.layerCount = 1,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachment,
		.pDepthAttachment = &depthAttachment,

	};

	commandBuffer.beginRendering(renderingInfo);
	commandBuffer.setScissor(
		0,
		{
			vk::Rect2D { { 0, 0 }, { 800, 600 } }
    }
	);

	commandBuffer.setViewport(
		0,
		{
			vk::Viewport { 0, 0, 800, 600, 0, 1 }
    }
	);
	commandBuffer.bindPipeline(
		vk::PipelineBindPoint::eGraphics, m_material.pipeline.m_pipeline
	);

	Buffer& vertexBuffer = resources.buffers["vertex_buffer"];
	Buffer& indexBuffer = resources.buffers["index_buffer"];

	commandBuffer.bindVertexBuffers(0, { vertexBuffer.buffer }, { 0 });
	commandBuffer.bindIndexBuffer(
		indexBuffer.buffer, 0, vk::IndexType::eUint16
	);

	commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		m_material.pipeline.m_pipelineLayout,
		0,
		{ m_material.globalSets[resources.currentFrame].set },
		{}
	);
}