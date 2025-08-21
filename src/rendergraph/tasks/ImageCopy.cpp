#include "ImageCopy.hpp"

#include <cstdint>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphBuilder.hpp"

void ImageCopy::setup(
	std::unordered_map<std::string_view, ResourceDependency>& images,
	std::unordered_map<std::string_view, ResourceDependency>& buffers
) {
	images[m_origin] = {
		.usage =  {
				  .type = ResourceUsage::Type::READ,
				  .access = vk::AccessFlagBits2::eTransferRead,
				  .stage = vk::PipelineStageFlagBits2::eTransfer,
				  },
		.requiredLayout = vk::ImageLayout::eTransferSrcOptimal
	};

	images[m_destination] = {
		.usage = {
				  .type = ResourceUsage::Type::WRITE,
				  .access = vk::AccessFlagBits2::eTransferWrite,
				  .stage = vk::PipelineStageFlagBits2::eTransfer,
				  },
		.requiredLayout = vk::ImageLayout::eTransferDstOptimal
	};
};

void ImageCopy::execute(
	vk::CommandBuffer& commandBuffer, const Resources& resources
) {
	Image& origin = resources.resourceManager.getNamedImage(m_origin);
	Image& destination = resources.resourceManager.getNamedImage(m_destination);

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
