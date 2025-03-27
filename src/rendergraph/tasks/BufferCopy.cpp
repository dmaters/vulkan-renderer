#include "BufferCopy.hpp"

#include <vulkan/vulkan_enums.hpp>

#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphResourceSolver.hpp"

void BufferCopy::setup(RenderGraphResourceSolver& graph) {
	graph.registerDependency(RenderGraphResourceSolver::BufferDependencyInfo {
		.name = m_info.origin.name,
		.usage = {
				  .type = ResourceUsage::Type::READ,
				  .access = vk::AccessFlagBits2::eTransferRead,
				  .stage = vk::PipelineStageFlagBits2::eTransfer,
				  
				  }
				
    });

	graph.registerDependency(RenderGraphResourceSolver::BufferDependencyInfo {
		.name = m_info.destination.name,
		.usage = {
				  .type = ResourceUsage::Type::WRITE,
				  .access = vk::AccessFlagBits2::eTransferWrite,
				  .stage = vk::PipelineStageFlagBits2::eTransfer,
				  },
				 
				});
}
void BufferCopy::execute(
	vk::CommandBuffer& buffer, const Resources& resources
) {
	Buffer& origin =
		resources.resourceManager.getNamedBuffer(m_info.origin.name);

	Buffer& destination =
		resources.resourceManager.getNamedBuffer(m_info.destination.name);

	uint32_t originBaseOffset =
		origin.transient ? origin.bufferAccess[resources.currentFrame].offset
						 : 0;
	uint32_t destinationBaseOffset =
		destination.transient
			? destination.bufferAccess[resources.currentFrame].offset
			: 0;
	buffer.copyBuffer(
		origin.buffer,
		destination.buffer,
		{
			{
             .srcOffset = originBaseOffset + m_info.origin.offset,
             .dstOffset = destinationBaseOffset + m_info.destination.offset,
             .size = m_info.origin.length,
			 },
    }
	);
}
