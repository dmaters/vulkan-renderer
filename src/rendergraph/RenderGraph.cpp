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

#include "Instance.hpp"
#include "RenderGraph.hpp"
#include "RenderGraphBuilder.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "resources/Image.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/ImageCopy.hpp"

RenderGraph::RenderGraph(
	Swapchain& swapchain,
	ResourceManager& resourceManager,
	MaterialManager& materialManager
) :
	m_swapchain(swapchain),
	m_resourceManager(resourceManager),
	m_materialManager(materialManager) {
	Instance& instance = Instance::Get();
	m_graphicQueue = Instance::Get().device.getQueue(
		instance.queueFamiliesIndices.graphicsIndex, 0
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
		Image& image = m_resourceManager.getNamedImage(name);

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
		Buffer& buffer = m_resourceManager.getNamedBuffer(name);
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
	Image& resultImage = m_resourceManager.getNamedImage("main_color");
	Image& swapchainImage = m_swapchain.getImage(index);

	vk::ImageMemoryBarrier2 sourceBarrier = {
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
		.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.newLayout = vk::ImageLayout::eTransferSrcOptimal,
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
		vk::ImageLayout::eTransferSrcOptimal,
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
	if (m_swapchain.getResolution() == vk::Extent2D(0)) return;

	for (auto& [name, _] : m_data.imageStatuses) {
		if (!m_swapchainDependentImages.contains(name)) continue;
		m_unusedImages[2].push_back(m_resourceManager.getNamedImageHandle(name)
		);

		vk::Extent2D resolution = m_swapchain.getResolution();
		ResourceManager::ImageDescription info = m_imageCreationInfos[name];
		uint8_t multiplier = m_swapchainDependentImages[name];
		float ratio = multiplier >= 1 ? (1 / multiplier) : (1 * -multiplier);

		info.width = resolution.width * ratio;
		info.height = resolution.height * ratio;

		ImageHandle image = m_resourceManager.createImage(info);

		m_resourceManager.setName(name, image);
	}

	m_swapchainFlushCounter = 2;
}

void RenderGraph::writeInitialSyncronizationBarrier(vk::CommandBuffer& buffer) {
	std::vector<vk::ImageMemoryBarrier2> imageBarriers;

	for (auto& [name, statuses] : m_data.imageStatuses) {
		auto& [initialLayout, finalLayout] = statuses;

		Image& image = m_resourceManager.getNamedImage(name);
		uint8_t accessIndex = image.transient ? m_currentFrame : 0;

		if (image.accesses[accessIndex].layout == initialLayout) continue;

		imageBarriers.push_back({
		
			.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
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

		image.accesses[accessIndex].layout = initialLayout;
	}

	buffer.pipelineBarrier2(vk::DependencyInfo {
		.imageMemoryBarrierCount = (uint32_t)imageBarriers.size(),
		.pImageMemoryBarriers = imageBarriers.data(),
	});
}

void RenderGraph::submit(const std::vector<Primitive>& primitives) {
	clearUnusedResources();

	vk::Device& device = Instance::Get().device;

	const Frame& frame = m_swapchain.getFrame(m_currentFrame);

	auto _ = device.waitForFences({ frame.fence }, vk::True, 1000);
	device.resetFences(frame.fence);

	vk::AcquireNextImageInfoKHR acquireInfo;
	acquireInfo.swapchain = m_swapchain.getSwapchain();
	acquireInfo.timeout = 1500;
	acquireInfo.semaphore = frame.imageAvailable;
	acquireInfo.fence = nullptr;
	acquireInfo.deviceMask = 1;

	uint32_t imageIndex = 0;
	try {
		imageIndex = device.acquireNextImage2KHR(acquireInfo).value;
	} catch (vk::OutOfDateKHRError& _) {
		rebuildSwapchain();
		return;
	}

	vk::CommandBufferAllocateInfo commandInfo;

	commandInfo.commandBufferCount = 1;
	commandInfo.commandPool = frame.commandPool;
	commandInfo.level = vk::CommandBufferLevel::ePrimary;

	auto commandBuffer = device.allocateCommandBuffers(commandInfo)[0];
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
		return;
	}

	for (auto& [name, statuses] : m_data.imageStatuses) {
		Image& image = m_resourceManager.getNamedImage(name);
		uint8_t accessIndex = image.transient ? m_currentFrame : 0;
		image.accesses[accessIndex].layout = statuses.finalLayout;
	}

	m_currentFrame = (m_currentFrame + 1) % 3;
}

void RenderGraph::build(GraphData graphData) {
	m_data = graphData;

	for (auto& image : m_data.transientImagesRequired) {
		if (image == "result") continue;

		ImageHandle handle =
			m_resourceManager.createImage(m_imageCreationInfos[image]);
		m_resourceManager.setName(image, handle);
	}

	for (auto& buffer : m_data.transientBuffersRequired) {
		BufferHandle handle =
			m_resourceManager.createBuffer(m_bufferCreationsInfos[buffer]);

		m_resourceManager.setName(buffer, handle);
	}
}