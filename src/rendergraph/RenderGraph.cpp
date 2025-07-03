#include "RenderGraph.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "RenderGraph.hpp"
#include "RenderGraphBuilder.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "resources/Image.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/ImageCopy.hpp"

RenderGraph::RenderGraph(
	Instance& instance,
	Swapchain& swapchain,
	ResourceManager& resourceManager,
	MaterialManager& materialManager
) :
	m_instance(instance),
	m_swapchain(swapchain),
	m_resourceManager(resourceManager),
	m_materialManager(materialManager) {
	m_graphicQueue = m_instance.device.getQueue(
		m_instance.queueFamiliesIndices.graphicsIndex, 0
	);
}
void RenderGraph::addImage(
	std::string_view name,
	ResourceManager::ImageDescription description,
	uint8_t swapchainResolutionMultiplier
) {
	m_imageCreationInfos[name] = description;

	if (swapchainResolutionMultiplier != 0)
		m_swapchainDependentImages[name] = swapchainResolutionMultiplier;
}

void RenderGraph::addBuffer(
	std::string_view name, ResourceManager::BufferDescription description
) {
	m_bufferCreationsInfos[name] = description;
}

void RenderGraph::writeMemoryBarrier(
	vk::CommandBuffer& commandBuffer, GraphData::TaskData& task
) const {
	std::vector<vk::ImageMemoryBarrier2> imageBarriers;
	for (auto& [name, barrier] : task.barriers.imageBarriers) {
		Image& image = m_resourceManager.getImage(m_images.at(name));

		uint8_t accessIndex = image.transient ? m_currentFrame : 0;

		barrier.image = image.image;
		barrier.subresourceRange = {
			.aspectMask = image.getAspectFlags(),
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = accessIndex,
			.layerCount = 1,
		};
		imageBarriers.push_back(barrier);
	}
	std::vector<vk::BufferMemoryBarrier2> bufferBarriers;

	for (auto& [name, barrier] : task.barriers.bufferBarriers) {
		Buffer& buffer = m_resourceManager.getBuffer(m_buffers.at(name));
		uint8_t accessIndex = buffer.transient ? m_currentFrame : 0;

		barrier.offset = buffer.bufferAccess[accessIndex].offset;
		bufferBarriers.push_back(barrier);
	}

	if (imageBarriers.size() == 0 && bufferBarriers.size() == 0) return;

	commandBuffer.pipelineBarrier2(vk::DependencyInfo {
		.bufferMemoryBarrierCount = (uint32_t)bufferBarriers.size(),
		.pBufferMemoryBarriers = bufferBarriers.data(),
		.imageMemoryBarrierCount = (uint32_t)imageBarriers.size(),
		.pImageMemoryBarriers = imageBarriers.data(),

	});
}

void RenderGraph::outputToSwapchain(
	vk::CommandBuffer& commandBuffer, uint32_t index
) {
	Image& resultImage = m_resourceManager.getImage(m_images.at("main_color"));
	Image& swapchainImage = m_swapchain.getImage(index);

	vk::ImageMemoryBarrier2 sourceBarrier = {
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
		.oldLayout = resultImage.accesses[m_currentFrame].layout,
		.newLayout = vk::ImageLayout::eTransferDstOptimal,
		.image = resultImage.image,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = m_currentFrame,
			.layerCount = 1,
		}
	};

	vk::ImageMemoryBarrier2 swapchainBarrier = {
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.oldLayout = swapchainImage.accesses[0].layout,
		.newLayout = vk::ImageLayout::eTransferDstOptimal,
		.image = swapchainImage.image,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	commandBuffer.pipelineBarrier2(vk::DependencyInfo {
		.imageMemoryBarrierCount = 2,
		.pImageMemoryBarriers =
			std::array<vk::ImageMemoryBarrier2, 2> { sourceBarrier,
                                                    swapchainBarrier }
				.data()
    });
	commandBuffer.blitImage(
		resultImage.image,
		resultImage.accesses[m_currentFrame].layout,
		swapchainImage.image,
		vk::ImageLayout::eTransferDstOptimal,
		{ vk::ImageBlit { 
			.srcSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.mipLevel = 0,
				.baseArrayLayer = m_currentFrame,
				.layerCount = 1,
			},
			.srcOffsets = {{
				vk::Offset3D{
				0,0,0
				},
			vk::Offset3D{
				(int32_t)swapchainImage.size.width,
				(int32_t)swapchainImage.size.height,
				1
			}}},
			.dstSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.mipLevel = 0,
				.baseArrayLayer = 0, 
				.layerCount = 1,
			},
			.dstOffsets = {{
				vk::Offset3D{
				0,0,0
				},
			vk::Offset3D{
				(int32_t)swapchainImage.size.width,
				(int32_t)swapchainImage.size.height,
				1
			}}},
		
		 	}
		},
		vk::Filter::eLinear
	);

	sourceBarrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
	sourceBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
	sourceBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead;
	sourceBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
	sourceBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
	sourceBarrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;

	swapchainBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	swapchainBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
	swapchainBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
	swapchainBarrier.dstAccessMask = vk::AccessFlagBits2::eNone;
	swapchainBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
	swapchainBarrier.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;

	commandBuffer.pipelineBarrier2(vk::DependencyInfo {
		.imageMemoryBarrierCount = 2,
		.pImageMemoryBarriers =
			std::array<vk::ImageMemoryBarrier2, 2> { sourceBarrier,
                                                    swapchainBarrier }
				.data()
    });
	resultImage.accesses[m_currentFrame].layout =
		vk::ImageLayout::eColorAttachmentOptimal;
	swapchainImage.accesses[0].layout = vk::ImageLayout::ePresentSrcKHR;
}

void RenderGraph::clearUnusedResources() {
	for (ImageHandle image : m_unusedImages[0]) {
		m_resourceManager.free(image);
	}

	for (BufferHandle buffer : m_unusedBuffers[0]) {
		m_resourceManager.free(buffer);
	}

	for (int i = 0; i < 2; i++) {
		m_unusedImages[i].clear();
		for (auto image : m_unusedImages[i + 1]) {
			m_unusedImages[i].push_back(image);
		}
	}
	m_unusedImages[2].clear();

	for (int i = 0; i < 2; i++) {
		m_unusedBuffers[i].clear();
		for (auto buffer : m_unusedBuffers[i + 1]) {
			m_unusedBuffers[i].push_back(buffer);
		}
	}
	m_unusedBuffers[2].clear();

	if (m_swapchainFlushCounter > 0)
		m_swapchainFlushCounter--;
	else
		m_swapchain.flush();
}

void RenderGraph::rebuildSwapchain() {
	m_swapchain.rebuild();

	for (auto& [name, _] : m_images) {
		if (!m_swapchainDependentImages.contains(name) ||
		    !m_images.contains(name))
			continue;
		m_unusedImages[2].push_back(m_resourceManager.getNamedImageHandle(name)
		);

		vk::Extent2D resolution = m_swapchain.getResolution();
		ResourceManager::ImageDescription info = m_imageCreationInfos[name];
		uint8_t multiplier = m_swapchainDependentImages[name];
		float ratio = multiplier > 1 ? (1 / multiplier) : (1 * -multiplier);

		info.width = resolution.width * ratio;
		info.height = resolution.height * ratio;

		m_resourceManager.createImage(name, info);
	}

	m_swapchainFlushCounter = 2;
}

void RenderGraph::writeInitialSyncronizationBarrier(vk::CommandBuffer& buffer) {
	std::vector<vk::ImageMemoryBarrier2> imageBarriers;

	for (auto& [name, statuses] : m_data.imageStatuses) {
		auto& [initialLayout, finalLayout] = statuses;

		Image& image = m_resourceManager.getImage(m_images[name]);
		uint8_t accessIndex = image.transient ? m_currentFrame : 0;

		if (image.accesses[accessIndex].layout == initialLayout) continue;

		imageBarriers.push_back({
			.oldLayout = image.accesses[accessIndex].layout,
			.newLayout = initialLayout,
			.image = image.image,
			.subresourceRange = {
								 .aspectMask = image.getAspectFlags(),
								 .baseMipLevel = 0,
								 .levelCount = 1,
								 .baseArrayLayer = accessIndex,
								 .layerCount = 1,
								 },
        });

		image.accesses[accessIndex].layout = finalLayout;
	}

	buffer.pipelineBarrier2(vk::DependencyInfo {
		.imageMemoryBarrierCount = (uint32_t)imageBarriers.size(),
		.pImageMemoryBarriers = imageBarriers.data(),
	});
}

void RenderGraph::submit(const std::vector<Primitive>& primitives) {
	clearUnusedResources();

	const Frame& frame = m_swapchain.getFrame(m_currentFrame);

	auto _ = m_instance.device.waitForFences({ frame.fence }, vk::True, 1000);
	m_instance.device.resetFences(frame.fence);

	vk::CommandBufferAllocateInfo commandInfo;

	commandInfo.commandBufferCount = 1;
	commandInfo.commandPool = frame.commandPool;
	commandInfo.level = vk::CommandBufferLevel::ePrimary;

	auto commandBuffer =
		m_instance.device.allocateCommandBuffers(commandInfo)[0];
	commandBuffer.begin(vk::CommandBufferBeginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
	});

	writeInitialSyncronizationBarrier(commandBuffer);

	const Resources resources {
		.resourceManager = m_resourceManager,
		.materialManager = m_materialManager,
		.primitives = primitives,
		.currentFrame = m_currentFrame,
	};

	for (auto& node : m_data.tasks) {
		writeMemoryBarrier(commandBuffer, node);

		std::visit(
			[&commandBuffer, &resources](auto&& t) {
				t.execute(commandBuffer, resources);
			},
			node.task
		);
	}

	vk::AcquireNextImageInfoKHR acquireInfo;
	acquireInfo.swapchain = m_swapchain.getSwapchain();
	acquireInfo.timeout = 1500;
	acquireInfo.semaphore = frame.imageAvailable;
	acquireInfo.fence = nullptr;
	acquireInfo.deviceMask = 1;
	uint32_t imageIndex = 0;
	try {
		imageIndex = m_instance.device.acquireNextImage2KHR(acquireInfo).value;

	} catch (vk::OutOfDateKHRError& _) {
		rebuildSwapchain();
	}

	outputToSwapchain(commandBuffer, imageIndex);

	commandBuffer.end();
	vk::SubmitInfo submitInfo;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = { &frame.imageAvailable };
	auto stage = vk::Flags(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	submitInfo.pWaitDstStageMask = { &stage };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = { &commandBuffer };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = { &frame.renderFinished };

	m_graphicQueue.submit({ submitInfo }, { frame.fence });

	vk::PresentInfoKHR presentInfo {};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.renderFinished;

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = { &(m_swapchain.getSwapchain()) };

	presentInfo.pImageIndices = &imageIndex;
	try {
		auto _ = m_graphicQueue.presentKHR(presentInfo);
	} catch (const vk::OutOfDateKHRError& _) {
		rebuildSwapchain();
	}
	m_currentFrame = (m_currentFrame + 1) % 3;
}

void RenderGraph::build(GraphData graphData) {
	m_data = graphData;

	for (auto& image : m_data.transientImagesRequired) {
		if (image == "result") continue;

		m_images[image] =
			m_resourceManager.createImage(m_imageCreationInfos[image]);
	}

	for (auto& buffer : m_data.transientImagesRequired) {
		m_buffers[buffer] =
			m_resourceManager.createBuffer(m_bufferCreationsInfos[buffer]);
	}
}