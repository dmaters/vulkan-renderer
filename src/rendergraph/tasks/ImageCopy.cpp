#include "ImageCopy.hpp"

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
	bool isOriginTransient = resources.transientImages.contains(m_origin);
	Image& origin =
		isOriginTransient
			? resources.transientImages[m_origin][resources.currentFrame]
			: resources.images[m_origin];

	bool isDestinationTransient =
		resources.transientImages.contains(m_destination);
	Image& destination =
		isDestinationTransient
			? resources.transientImages[m_destination][resources.currentFrame]
			: resources.images[m_destination];

	vk::ImageSubresourceLayers layer = {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.mipLevel = 0,
		.baseArrayLayer = 0,
		.layerCount = 1,
	};
	commandBuffer.copyImage(
		origin.image,
		origin.layout,
		destination.image,
		destination.layout,
		{
			{
             .srcSubresource = layer,
             .dstSubresource = layer,
             .extent = origin.size,
			 },
    }

	);
}
