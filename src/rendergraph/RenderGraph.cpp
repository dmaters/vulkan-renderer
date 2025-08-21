#include "RenderGraph.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
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
#include "material/MaterialDefinitions.hpp"
#include "material/MaterialManager.hpp"
#include "resources/Buffer.hpp"
#include "resources/Image.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/ImageCopy.hpp"
#include "tasks/RenderPass.hpp"

RenderGraph::RenderGraph(
	Swapchain& swapchain,
	ResourceManager& resourceManager,
	MaterialManager& materialManager
) :
	m_swapchain(swapchain),
	m_resourceManager(resourceManager),
	m_materialManager(materialManager) {
	vk::Device& device = Instance::Get().device;
	m_renderSemaphores = {
		device.createSemaphore({}),
		device.createSemaphore({}),
		device.createSemaphore({}),
	};
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

		barrier.image = image.image;
		barrier.subresourceRange = {
			.aspectMask = Image::GetAspectFlags(image.format),
			.levelCount = 1,
			.layerCount = 1,
		};
		imageBarriers.push_back(barrier);
	}
	std::vector<vk::BufferMemoryBarrier2> bufferBarriers;

	for (auto& [name, barrier] : task.barriers.bufferBarriers) {
		Buffer& buffer = m_resourceManager.getNamedBuffer(name);
		barrier.buffer = buffer.buffer;
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
		.srcStageMask =  vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.srcAccessMask =vk::AccessFlagBits2::eColorAttachmentWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
		.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.newLayout = vk::ImageLayout::eTransferSrcOptimal,
		.image = resultImage.image,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	vk::ImageMemoryBarrier2 swapchainBarrier = {
		.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.oldLayout = swapchainImage.layout,
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
			std::array<vk::ImageMemoryBarrier2, 2> {
													sourceBarrier, swapchainBarrier,
													}
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
				.baseArrayLayer = 0,
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
	resultImage.layout = vk::ImageLayout::eColorAttachmentOptimal;
	swapchainImage.layout = vk::ImageLayout::ePresentSrcKHR;
}

void RenderGraph::clearUnusedResources() {
	for (ImageHandle image : m_unusedImages[0]) {
		//		m_resourceManager.free(image);
	}

	for (BufferHandle buffer : m_unusedBuffers[0]) {
		//		m_resourceManager.free(buffer);
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
	/*
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

	*/
	m_swapchainFlushCounter = 2;
}

void RenderGraph::writeInitialSyncronizationBarrier(vk::CommandBuffer& buffer) {
	std::vector<vk::ImageMemoryBarrier2> imageBarriers;

	for (auto& [name, statuses] : m_data.imageStatuses) {
		auto& [initialLayout, finalLayout] = statuses;

		Image& image = m_resourceManager.getNamedImage(name);

		// if (image.layout == initialLayout) continue;

		imageBarriers.push_back({
			.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
			.dstStageMask = vk::PipelineStageFlagBits2::eAllGraphics,
			.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite |
                 vk::AccessFlagBits2::eDepthStencilAttachmentWrite | 
				 vk::AccessFlagBits2::eShaderRead,
			.oldLayout = image.layout,
			.newLayout = initialLayout,
			.image = image.image,
			.subresourceRange = {
								 .aspectMask = image.getAspectFlags(),
								 .baseMipLevel = 0,
								 .levelCount = 1,
								 .baseArrayLayer = 0,
								 .layerCount = 1,
								 },
        });

		image.layout = initialLayout;
	}

	Buffer& projViewBuffer = m_resourceManager.getNamedBuffer("camera_data");
	vk::BufferMemoryBarrier2 projViewUpdate {
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.buffer = projViewBuffer.buffer,
		.offset = 0,
		.size = projViewBuffer.size,

	};

	buffer.pipelineBarrier2(vk::DependencyInfo {
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers = &projViewUpdate,
		.imageMemoryBarrierCount = (uint32_t)imageBarriers.size(),
		.pImageMemoryBarriers = imageBarriers.data(),

	});
}

void RenderGraph::submit(const Scene& scene) {
	clearUnusedResources();

	vk::Device& device = Instance::Get().device;

	const Frame& frame = m_swapchain.getFrame(m_currentFrame);

	auto _ = device.waitForFences({ frame.fence }, vk::True, UINT64_MAX);

	device.resetFences(frame.fence);
	device.resetCommandPool(frame.commandPool);

	vk::AcquireNextImageInfoKHR acquireInfo;
	acquireInfo.swapchain = m_swapchain.getSwapchain();
	acquireInfo.timeout = 1e12;
	acquireInfo.semaphore = frame.imageAvailable;
	acquireInfo.fence = nullptr;
	acquireInfo.deviceMask = 1;

	uint32_t imageIndex = 0;
	try {
		auto result = device.acquireNextImage2KHR(acquireInfo);
		imageIndex = result.value;

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

	MaterialDefinitions::Camera cameraView {
		.view = scene.camera.getViewMatrix(),
		.projection = scene.camera.getProjectionMatrix(),
		.frustumBounds = scene.camera.getFrustumBounds(),
		.frustumPlanes = scene.camera.getFrustum(),
	};

	Buffer& projViewBuffer = m_resourceManager.getNamedBuffer("camera_data");

	commandBuffer.updateBuffer(
		projViewBuffer.buffer, 0, sizeof(cameraView), &cameraView
	);

	writeInitialSyncronizationBarrier(commandBuffer);

	const Resources resources {
		.resourceManager = m_resourceManager,
		.materialManager = m_materialManager,
		.primitives = scene.primitives,
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

	auto semaphore = m_renderSemaphores[imageIndex];

	vk::SubmitInfo submitInfo;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = { &frame.imageAvailable };
	auto stage = vk::Flags(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	submitInfo.pWaitDstStageMask = { &stage };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = { &commandBuffer };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = { &semaphore };

	Instance::Get().graphicQueue.submit({ submitInfo }, { frame.fence });

	vk::PresentInfoKHR presentInfo {};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &semaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = { &(m_swapchain.getSwapchain()) };
	presentInfo.pImageIndices = &imageIndex;
	try {
		auto _ = Instance::Get().presentQueue.presentKHR(presentInfo);
	} catch (const vk::OutOfDateKHRError& _) {
		rebuildSwapchain();
		return;
	}

	for (auto& [name, statuses] : m_data.imageStatuses) {
		Image& image = m_resourceManager.getNamedImage(name);
		image.layout = statuses.finalLayout;
	}

	m_currentFrame = (m_currentFrame + 1) % 3;
}

void RenderGraph::build(GraphData graphData) {
	m_data = graphData;

	m_data.transientImagesRequired.erase("result");

	std::vector<ResourceManager::ImageDescription> imagesDescription;
	std::vector<ResourceManager::ImageDescription>
		resolutionDependentImagesDescription;
	std::vector<ResourceManager::BufferDescription> buffersDescription;
	for (auto& image : graphData.transientImagesRequired) {
		if (m_swapchainDependentImages.contains(image)) {
			resolutionDependentImagesDescription.push_back(
				m_imageCreationInfos.at(image)
			);
		} else
			imagesDescription.push_back(m_imageCreationInfos.at(image));
	}
	for (auto& image : graphData.transientBuffersRequired) {
		buffersDescription.push_back(m_bufferCreationsInfos.at(image));
	}

	m_frameDataAllocation = m_resourceManager.createResources(
		imagesDescription, buffersDescription
	);
	auto& images = m_resourceManager.getImages(m_frameDataAllocation);

	uint32_t index = 0;
	for (auto& image : graphData.transientImagesRequired) {
		if (m_swapchainDependentImages.contains(image)) continue;
		m_resourceManager.setName(image, images.at(index));
		index++;
	}
	index = 0;

	auto& buffers = m_resourceManager.getBuffers(m_frameDataAllocation);
	for (auto& buffer : graphData.transientBuffersRequired) {
		m_resourceManager.setName(buffer, buffers.at(index));
		index++;
	}

	m_resolutionDependentAllocation = m_resourceManager.createResources(
		resolutionDependentImagesDescription, {}
	);

	index = 0;

	auto& resolutionDependentImages =
		m_resourceManager.getImages(m_resolutionDependentAllocation);
	for (auto& image : graphData.transientImagesRequired) {
		if (!m_swapchainDependentImages.contains(image)) continue;
		m_resourceManager.setName(image, resolutionDependentImages.at(index));
		index++;
	}
}
