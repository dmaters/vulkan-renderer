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
	std::string_view originName = m_info.origin.name;
	vk::Buffer origin;

	if (resources.buffers.contains(originName))
		origin = resources.buffers[originName].buffer;
	else
		origin = resources.transientBuffers[originName][resources.currentFrame]
		             .buffer;

	std::string_view destinationName = m_info.destination.name;
	vk::Buffer destination;

	if (resources.buffers.contains(destinationName))
		destination = resources.buffers[destinationName].buffer;
	else
		destination =
			resources.transientBuffers[destinationName][resources.currentFrame]
				.buffer;

	buffer.copyBuffer(

		origin,
		destination,
		{
			{
             .srcOffset = m_info.origin.offset,
             .dstOffset = m_info.destination.offset,
             .size = m_info.origin.length,
			 },
    }
	);
}