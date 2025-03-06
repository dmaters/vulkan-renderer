#include "RenderPass.hpp"

#include <map>
#include <vulkan/vulkan_handles.hpp>

#include "rendergraph/RenderGraph.hpp"

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
}