#include "OpaquePass.hpp"

#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "RenderPass.hpp"
#include "material/MaterialManager.hpp"
#include "material/Pipeline.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphResourceSolver.hpp"
#include "resources/ResourceManager.hpp"

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

	renderGraph
		.registerDependency(RenderGraphResourceSolver::BufferDependencyInfo {
			.name = "gset_buffer",
			.usage = {
					  .type = ResourceUsage::Type::READ,
					  .access = vk::AccessFlagBits2::eShaderRead,
					  .stage = vk::PipelineStageFlagBits2::eVertexShader,
					  }
    });
	RenderPass::setAttachments({
		.color = "main_color",
		.depth = "main_depth",
	});
}

void OpaquePass::execute(
	vk::CommandBuffer& commandBuffer, const Resources& resources
) {
	RenderPass::execute(commandBuffer, resources);

	MaterialManager::Material& baseMaterial =
		resources.primitives[0].material.baseMaterial;
	Pipeline pipeline = baseMaterial.pipeline;

	commandBuffer.bindPipeline(
		vk::PipelineBindPoint::eGraphics, baseMaterial.pipeline.m_pipeline
	);

	commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipeline.m_pipelineLayout,
		0,
		baseMaterial.globalSet.set,
		nullptr
	);
	for (auto primitive : resources.primitives) {
		commandBuffer.pushConstants(
			pipeline.m_pipelineLayout,
			vk::ShaderStageFlagBits::eVertex,
			0,
			64,
			&primitive.modelMatrix
		);

		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pipeline.m_pipelineLayout,
			1,
			{ primitive.material.instanceSet.set },
			nullptr
		);

		commandBuffer.bindVertexBuffers(
			0, { primitive.vertexBuffer.buffer }, { 0 }
		);
		commandBuffer.bindIndexBuffer(
			primitive.indexBuffer.buffer, 0, vk::IndexType::eUint32
		);

		commandBuffer.drawIndexed(primitive.indexCount, 1, 0, 0, 0);
	}

	commandBuffer.endRendering();
}