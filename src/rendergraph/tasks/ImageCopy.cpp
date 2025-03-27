#include "ImageCopy.hpp"

#include <cstdint>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "../RenderGraphResourceSolver.hpp"
#include "rendergraph/RenderGraph.hpp"

void ImageCopy::setup(RenderGraphResourceSolver& solver) {
	solver.registerDependency(RenderGraphResourceSolver::ImageDependencyInfo {
		.name = m_origin,
		.usage =  {
				  .type = ResourceUsage::Type::READ,
				  .access = vk::AccessFlagBits2::eTransferRead,
				  .stage = vk::PipelineStageFlagBits2::eTransfer,
				  },
		.requiredLayout = vk::ImageLayout::eTransferSrcOptimal
	});

	solver.registerDependency(RenderGraphResourceSolver::ImageDependencyInfo {
		.name = m_destination,
		.usage = {
				  .type = ResourceUsage::Type::WRITE,
				  .access = vk::AccessFlagBits2::eTransferWrite,
				  .stage = vk::PipelineStageFlagBits2::eTransfer,
				  },
		.requiredLayout = vk::ImageLayout::eTransferDstOptimal
	});
};

void ImageCopy::execute(
	vk::CommandBuffer& commandBuffer, const Resources& resources
) {
	Image& origin = resources.resourceManager.getNamedImage(m_origin);

	uint8_t originAccessIndex = origin.transient ? resources.currentFrame : 0;
	Image& destination = resources.resourceManager.getNamedImage(m_destination);

	uint8_t destinationAccessIndex =
		destination.transient ? resources.currentFrame : 0;

	vk::ImageSubresourceLayers originLayer = {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.mipLevel = 0,
		.baseArrayLayer = originAccessIndex,
		.layerCount = 1,
	};
	vk::ImageSubresourceLayers destinationLayer = {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.mipLevel = 0,
		.baseArrayLayer = destinationAccessIndex,
		.layerCount = 1,
	};
	commandBuffer.copyImage(
		origin.image,
		origin.accesses[originAccessIndex].layout,
		destination.image,
		destination.accesses[destinationAccessIndex].layout,
		{
			{
             .srcSubresource = originLayer,
             .dstSubresource = destinationLayer,
             .extent = origin.size,
			 },
    }
	);
}
