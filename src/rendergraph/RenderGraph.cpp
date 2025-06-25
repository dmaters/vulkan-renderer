#include "RenderGraph.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "RenderGraph.hpp"
#include "RenderGraphBuilder.hpp"
#include "Swapchain.hpp"
#include "resources/Image.hpp"
#include "resources/ResourceManager.hpp"

RenderGraph::RenderGraph(
	Instance& instance, Swapchain& swapchain, ResourceManager& resourceManager
) :
	m_instance(instance),
	m_swapchain(swapchain),
	m_resourceManager(resourceManager) {
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

bool isWriteOperation(vk::AccessFlags2 flags) {
	return flags & vk::AccessFlagBits2::eColorAttachmentWrite ||
	       flags & vk::AccessFlagBits2::eDepthStencilAttachmentWrite ||
	       flags & vk::AccessFlagBits2::eTransferWrite;
}

bool isBarrierNeeded(vk::AccessFlags2 current, vk::AccessFlags2 operation) {
	return isWriteOperation(current) ^ isWriteOperation(operation);
}

bool RenderGraph::addImageBarrier(
	ImageDependencyInfo& imageReference, vk::ImageMemoryBarrier2& imageBarrier
) {
	Image& image = m_resourceManager.getNamedImage(imageReference.name);

	uint8_t accessIndex = image.transient ? m_currentFrame : 0;

	vk::AccessFlags2 currentAccess = image.accesses[accessIndex].accessType;
	vk::PipelineStageFlags2 currentStage =
		image.accesses[accessIndex].accessStage;

	image.accesses[accessIndex].accessType = imageReference.usage.access;
	image.accesses[accessIndex].accessStage = imageReference.usage.stage;

	if (!isBarrierNeeded(currentAccess, imageReference.usage.access) &&
	    image.accesses[accessIndex].layout ==
	        imageReference.requiredLayout.value_or(
				image.accesses[accessIndex].layout
			))
		return false;

	imageBarrier = vk::ImageMemoryBarrier2{
			.srcStageMask = currentStage,
			.srcAccessMask = currentAccess,
			.dstStageMask = imageReference.usage.stage,
			.dstAccessMask = imageReference.usage.access,
			.oldLayout = image.accesses[accessIndex].layout,
			.newLayout = imageReference.requiredLayout.value_or(image.accesses[accessIndex].layout),
			.image = image.image,
			.subresourceRange = {
							 .aspectMask =
		                             image.getAspectFlags(),
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = accessIndex,
							.layerCount = 1,
						},

		};
	image.accesses[accessIndex].layout = imageReference.requiredLayout.value_or(
		image.accesses[accessIndex].layout
	);

	return true;
}

void RenderGraph::addMemoryBarriers(
	vk::CommandBuffer& commandBuffer, std::string_view taskName
) {
	RegisteredTask& task = m_registeredTask[taskName];

	std::vector<vk::ImageMemoryBarrier2> imageBarriers;
	for (auto& imageReference : task.images) {
		Image& image = m_resourceManager.getNamedImage(imageReference.name);
		uint8_t accessIndex = image.transient ? m_currentFrame : 0;

		if (m_internalResources.contains(imageReference.name)) {
			image.accesses[accessIndex].accessStage =
				imageReference.usage.stage;
			image.accesses[accessIndex].accessType =
				imageReference.usage.access;
			image.accesses[accessIndex].layout =
				imageReference.requiredLayout.value_or(
					image.accesses[accessIndex].layout
				);
			continue;
		}

		vk::ImageMemoryBarrier2 barrier;
		if (addImageBarrier(imageReference, barrier))
			imageBarriers.push_back(barrier);
		image.accesses[accessIndex].layout =
			imageReference.requiredLayout.value_or(
				image.accesses[accessIndex].layout
			);
	}
	std::vector<vk::BufferMemoryBarrier2> bufferBarriers;

	for (auto& bufferReference : task.buffers) {
		Buffer& buffer = m_resourceManager.getNamedBuffer(bufferReference.name);
		uint8_t accessIndex = buffer.transient ? m_currentFrame : 0;

		buffer.bufferAccess[accessIndex].accessStage =
			bufferReference.usage.stage;
		vk::AccessFlags2 currentAccess =
			buffer.bufferAccess[accessIndex].accessType;
		buffer.bufferAccess[accessIndex].accessType =
			bufferReference.usage.access;

		if (m_internalResources.contains(bufferReference.name) ||
		    !isBarrierNeeded(currentAccess, bufferReference.usage.access))
			continue;

		bufferBarriers.push_back(vk::BufferMemoryBarrier2 {
			.srcStageMask = buffer.bufferAccess[accessIndex].accessStage,
			.srcAccessMask = buffer.bufferAccess[accessIndex].accessType,
			.dstStageMask = bufferReference.usage.stage,
			.dstAccessMask = bufferReference.usage.access,
			.buffer = buffer.buffer,
			.offset = buffer.bufferAccess[accessIndex].offset,
			.size = buffer.bufferAccess[accessIndex].length,
		});
	}
	if (task.barriers.has_value()) {
		auto& localImageBarriers =
			task.barriers.value()[m_currentFrame].imageBarriers;
		imageBarriers.insert(
			imageBarriers.end(),
			localImageBarriers.begin(),
			localImageBarriers.end()
		);

		auto& localBufferBarriers =
			task.barriers.value()[m_currentFrame].bufferBarriers;
		bufferBarriers.insert(
			bufferBarriers.end(),
			localBufferBarriers.begin(),
			localBufferBarriers.end()
		);
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

	vk::ImageMemoryBarrier2 sourceBarrier;
	ImageDependencyInfo sourceInfo = {
		.name = "main_color",
		.usage = {
			.type = ResourceUsage::Type::READ,
			.access = vk::AccessFlagBits2::eTransferRead,
			.stage = vk::PipelineStageFlagBits2::eTransfer,
		},
		.requiredLayout = vk::ImageLayout::eTransferSrcOptimal,
	};
	addImageBarrier(sourceInfo, sourceBarrier);

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

	for (auto& [name, info] : m_swapchainDependentImages) {
		m_unusedImages[2].push_back(m_resourceManager.getNamedImageHandle(name)
		);

		vk::Extent2D resolution = m_swapchain.getResolution();

		info.width = resolution.width;
		info.height = resolution.height;

		m_resourceManager.createImage(name, info);
	}
	m_initialized = false;
	m_swapchainFlushCounter = 2;
}

void RenderGraph::initializeExternalImages(vk::CommandBuffer& commandBuffer) {
	std::vector<vk::ImageMemoryBarrier2> imageBarriers;
	for (auto& [name, dependency] : m_uninitializedImages) {
		vk::ImageMemoryBarrier2 barrier;
		uint8_t currentFrame = m_currentFrame;
		for (int i = 0; i < 3; i++) {
			m_currentFrame = i;
			if (addImageBarrier(dependency, barrier)) {
				imageBarriers.push_back(barrier);
			}
		}

		m_currentFrame = currentFrame;
	}

	commandBuffer.pipelineBarrier2(vk::DependencyInfo {
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

	if (!m_initialized) {
		initializeExternalImages(commandBuffer);
		m_initialized = true;
	}

	const Resources resources {
		.resourceManager = m_resourceManager,
		.primitives = primitives,
		.currentFrame = m_currentFrame,
	};

	for (auto& node : m_nodes) {
		RegisteredTask& task = m_registeredTask[node];

		addMemoryBarriers(commandBuffer, node);
		task.task->execute(commandBuffer, resources);
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

std::optional<std::array<Barriers, 3>> buildBarrier(
	const std::unordered_map<std::string_view, vk::ImageMemoryBarrier2>&
		imageBarriers,
	const std::unordered_map<std::string_view, vk::BufferMemoryBarrier2>&
		bufferBarriers,
	ResourceManager& resourceManager
) {
	if (imageBarriers.size() == 0 && bufferBarriers.size() == 0)
		return std::nullopt;

	std::array<Barriers, 3> barriers;
	for (int i = 0; i < 3; i++) {
		auto& barrier = barriers[i];

		std::vector<vk::ImageMemoryBarrier2> compiledImageBarriers;

		for (auto& [name, imageBarrier] : imageBarriers) {
			auto compiledBarrier = imageBarrier;
			const auto& image = resourceManager.getNamedImage(name);
			compiledBarrier.image = image.image;
			uint8_t accessIndex = image.transient ? i : 0;

			compiledBarrier.subresourceRange = vk::ImageSubresourceRange {
				.aspectMask = image.getAspectFlags(),
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = accessIndex,
				.layerCount = 1,
			};

			compiledImageBarriers.push_back(compiledBarrier);
		}

		std::vector<vk::BufferMemoryBarrier2> compiledBufferBarriers;
		for (auto& [name, bufferBarrier] : bufferBarriers) {
			auto compiledBarrier = bufferBarrier;
			const auto& buffer = resourceManager.getNamedBuffer(name);
			compiledBarrier.buffer = buffer.buffer;
			uint8_t accessIndex = buffer.transient ? i : 0;

			compiledBarrier.offset = buffer.bufferAccess[accessIndex].offset;
			compiledBarrier.size = buffer.bufferAccess[accessIndex].length;

			compiledBufferBarriers.push_back(compiledBarrier);
		}

		barrier.imageBarriers = compiledImageBarriers;
		barrier.bufferBarriers = compiledBufferBarriers;
	}
	return barriers;
}

void RenderGraph::build(GraphData graphData) {
	m_nodes.clear();
	for (auto& taskData : graphData.tasks) {
		m_nodes.push_back(taskData.name);
		m_registeredTask[taskData.name] = taskData;
	}

	m_initialized = false;
}