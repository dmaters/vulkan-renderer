#include "OpaquePass.hpp"

#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Primitive.hpp"
#include "RenderPass.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphResourceSolver.hpp"

void OpaquePass::setup(RenderGraphResourceSolver& renderGraph) {
	renderGraph.registerDependency({
		.name = "main_color",
		.usage =
			 {
						   .type = ResourceUsage::Type::WRITE,
						   .access = vk::AccessFlagBits2::eColorAttachmentWrite,
						   .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
						   },
		.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
		
	});
	renderGraph.registerDependency({
		.name = "main_depth",
		.usage =  {
						   .type = ResourceUsage::Type::WRITE,
						   .access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
						   .stage = vk::PipelineStageFlagBits2::eLateFragmentTests,
						   },
		.requiredLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
						});

	RenderPass::setAttachments({
		.color = Attachment { "main_color", m_clear },
		.depth = Attachment { "main_depth", m_clear },
	});

	renderGraph.registerDependency(RenderGraphResourceSolver::BufferDependencyInfo {
		.name = "gset_buffer",
		.usage =  {
						   .type = ResourceUsage::Type::READ,
						   .access = vk::AccessFlagBits2::eShaderRead,
						   .stage = vk::PipelineStageFlagBits2::eVertexShader,
						   },
						});
}

void OpaquePass::execute(
	vk::CommandBuffer& commandBuffer, const Resources& resources
) {
	RenderPass::execute(commandBuffer, resources);

	for (auto primitive : resources.primitives) {
		commandBuffer.pushConstants(
			m_material->pipeline.pipelineLayout,
			vk::ShaderStageFlagBits::eVertex,
			0,
			64,
			&primitive.modelMatrix
		);

		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			m_material->pipeline.pipelineLayout,
			1,
			{ m_material->instanceSets[primitive.material.instanceIndex] },
			nullptr
		);

		commandBuffer.drawIndexed(
			primitive.indexCount,
			1,
			primitive.baseIndex,
			primitive.baseVertex,
			0
		);
	}
	commandBuffer.endRendering();
}