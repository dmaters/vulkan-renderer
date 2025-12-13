#include "RenderGraphRunner.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/GraphData.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"
using namespace rendergraph::internal;

RenderGraphRunner::RenderGraphRunner(
	const GraphData& data,
	Swapchain& swapchain,
	ResourceManager& resourceManager,
	MaterialManager& materialManager
) :
	m_data(data),
	m_swapchain(swapchain),
	m_resourceManager(resourceManager),
	m_materialManager(materialManager) {
	vk::Device& device = Instance::Get().device;
	m_renderSemaphores = {
		device.createSemaphore({}),
		device.createSemaphore({}),
		device.createSemaphore({}),
	};
	build();
}

void RenderGraphRunner::clearUnusedResources() {
	if (m_swapchainFlushCounter > 1)
		m_swapchainFlushCounter--;
	else if (m_swapchainFlushCounter == 1) {
		m_resourceManager.freeAllocation(m_oldResolutionDependentAllocation);
		m_swapchain.flush();
		m_swapchainFlushCounter = 0;
	}
}
void RenderGraphRunner::buildSwapchainResources() {
	std::vector<ResourceManager::ImageDescription>
		resolutionDependentImagesDescription;

	for (auto& [index, ratio] : m_data.swapchainImageRatio) {
		auto creationInfo = m_data.images.at(index);

		float multiplier = ratio > 0 ? ratio : (1 / -ratio);

		vk::Extent2D res = m_swapchain.getResolution();
		creationInfo.width = res.width * multiplier;
		creationInfo.height = res.height * multiplier;

		resolutionDependentImagesDescription.push_back(creationInfo);
	}

	m_resolutionDependentAllocation = m_resourceManager.createResources(
		resolutionDependentImagesDescription, {}
	);

	auto& images = m_resourceManager.getImages(m_resolutionDependentAllocation);
	int i = 0;
	for (auto& [index, _] : m_data.swapchainImageRatio) {
		m_images[index] = images[i];
		m_resourceManager.setName(m_data.resourceNames[index], images[i]);
		i++;
	}
}
void RenderGraphRunner::rebuildSwapchain() {
	m_swapchain.rebuild();
	if (m_swapchain.getResolution() == vk::Extent2D(0)) return;
	m_oldResolutionDependentAllocation = m_resolutionDependentAllocation;
	buildSwapchainResources();
	m_swapchainFlushCounter = 4;
	m_swapchainImagesInitialized = false;
}
void RenderGraphRunner::outputToSwapchain(
	vk::CommandBuffer& commandBuffer, ResourceIndex output, uint32_t imageIndex
) {
	Image& resultImage = m_resourceManager.getImage(m_images[output]);

	Image& swapchainImage = m_swapchain.getImage(imageIndex);

	vk::ImageMemoryBarrier2 sourceBarrier = {
		.srcStageMask =  vk::PipelineStageFlagBits2::eComputeShader,
		.srcAccessMask =vk::AccessFlagBits2::eShaderStorageWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
		.oldLayout = vk::ImageLayout::eGeneral,
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
		.oldLayout = vk::ImageLayout::eUndefined,
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

	commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.imageMemoryBarrierCount = 2,
			.pImageMemoryBarriers =
				std::array<vk::ImageMemoryBarrier2, 2> {
														sourceBarrier, swapchainBarrier,
														}
					.data()
    }
	);
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
	sourceBarrier.newLayout = vk::ImageLayout::eGeneral;
	sourceBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead;
	sourceBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderStorageWrite;
	sourceBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
	sourceBarrier.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;

	swapchainBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	swapchainBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
	swapchainBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
	swapchainBarrier.dstAccessMask = vk::AccessFlagBits2::eNone;
	swapchainBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
	swapchainBarrier.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;

	commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.imageMemoryBarrierCount = 2,
			.pImageMemoryBarriers =
				std::array<vk::ImageMemoryBarrier2, 2> { sourceBarrier,
                                                        swapchainBarrier }
					.data()
    }
	);
}
void updateCameraBuffer(
	vk::CommandBuffer& commandBuffer,
	const Scene& scene,
	ResourceManager& resourceManager
) {
	glm::mat4 view = scene.camera.getViewMatrix();
	glm::mat4 proj = scene.camera.getProjectionMatrix();

	MaterialDefinitions::Camera cameraView {
		.view = view,
		.projection = proj,
		.invView = glm::inverse(view),
		.invProj = glm::inverse(proj),
		.frustumPoints = scene.camera.getFrustumPoints(),
	};

	Buffer& projViewBuffer = resourceManager.getNamedBuffer("camera_data");

	commandBuffer.updateBuffer(
		projViewBuffer.buffer, 0, sizeof(cameraView), &cameraView
	);

	vk::BufferMemoryBarrier2 projViewUpdate {
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.buffer = projViewBuffer.buffer,
		.offset = 0,
		.size = projViewBuffer.size,

	};

	commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers = &projViewUpdate,
		}
	);
}

void RenderGraphRunner::submit(
	const Scene& scene, ResourceIndex output, ExecutionInfo& execInfo
) {
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
		auto [result, index] = device.acquireNextImage2KHR(acquireInfo);
		imageIndex = index;
		if (result == vk::Result::eSuboptimalKHR) rebuildSwapchain();

	} catch (vk::OutOfDateKHRError _) {
		rebuildSwapchain();
		return;
	}

	vk::CommandBufferAllocateInfo commandInfo;

	commandInfo.commandBufferCount = 1;
	commandInfo.commandPool = frame.commandPool;
	commandInfo.level = vk::CommandBufferLevel::ePrimary;

	auto commandBuffer = device.allocateCommandBuffers(commandInfo)[0];
	commandBuffer.begin(
		vk::CommandBufferBeginInfo {
			.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		}
	);

	updateCameraBuffer(commandBuffer, scene, m_resourceManager);
	std::vector<ResourceIndex> inputs;
	std::vector<ResourceIndex> outputs;

	uint32_t taskCount = m_data.tasks.size();
	for (auto taskIndex : execInfo.tasks) {
		inputs.clear();

		auto taskData = taskIndex < taskCount
		                    ? m_data.taskData[taskIndex]
		                    : execInfo.data[taskIndex - taskCount];

		for (int i = 0; i < taskData.inputs.count; i++) {
			inputs.push_back(
				m_data.taskDependencies[taskData.inputs.offset + i].first
			);
		}
		outputs.clear();
		for (int i = 0; i < taskData.outputs.count; i++) {
			outputs.push_back(
				m_data.taskDependencies[taskData.outputs.offset + i].first
			);
		}

		TaskContext context {
			.commandBuffer = commandBuffer,
			.inputs = inputs,
			.outputs = outputs,
			.images = m_images,
			.buffers = m_buffers,
			.resourceManager = m_resourceManager,
			.materialManager = m_materialManager,
			.scene = scene,
		};

		if (taskIndex < taskCount)
			m_data.tasks[taskIndex](context);
		else
			execInfo.barriers[taskIndex - taskCount](context);
	}
	outputToSwapchain(commandBuffer, output, imageIndex);

	commandBuffer.end();

	uint64_t semaphoreWaitValues[2] = { m_resourceManager.sync(), 0 };

	vk::Semaphore semaphores[2] = {
		m_resourceManager.getSemaphore(),
		frame.imageAvailable,
	};
	vk::TimelineSemaphoreSubmitInfo waitInfo {
		.waitSemaphoreValueCount = 2,
		.pWaitSemaphoreValues = semaphoreWaitValues,
	};
	vk::SubmitInfo submitInfo;
	submitInfo.pNext = &waitInfo;
	submitInfo.waitSemaphoreCount = 2;
	submitInfo.pWaitSemaphores = semaphores;
	vk::PipelineStageFlags stages[2] = {
		vk::PipelineStageFlagBits::eVertexInput,
		vk::PipelineStageFlagBits::eColorAttachmentOutput
	};
	submitInfo.pWaitDstStageMask = stages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = { &commandBuffer };
	submitInfo.signalSemaphoreCount = 1;
	vk::Semaphore renderSemaphore = m_renderSemaphores[imageIndex];
	submitInfo.pSignalSemaphores = { &renderSemaphore };

	Instance::Get().graphicQueue.submit({ submitInfo }, { frame.fence });

	vk::PresentInfoKHR presentInfo {};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = { &(m_swapchain.getSwapchain()) };
	presentInfo.pImageIndices = &imageIndex;

	try {
		auto presentResult =
			Instance::Get().presentQueue.presentKHR(presentInfo);
		if (presentResult == vk::Result::eSuboptimalKHR) rebuildSwapchain();

	} catch (vk::OutOfDateKHRError _) {
		rebuildSwapchain();
	}

	m_currentFrame = (m_currentFrame + 1) % 3;
}

void RenderGraphRunner::build() {
	std::vector<ResourceManager::ImageDescription> staticImagesDescriptions;
	std::vector<ResourceManager::BufferDescription> buffersDescription;
	for (auto& [index, description] : m_data.images) {
		if (m_data.swapchainImageRatio.contains(index)) continue;
		staticImagesDescriptions.push_back(description);
	}
	for (auto& [index, description] : m_data.buffers) {
		buffersDescription.push_back(description);
	}

	m_frameDataAllocation = m_resourceManager.createResources(
		staticImagesDescriptions, buffersDescription
	);
	auto& images = m_resourceManager.getImages(m_frameDataAllocation);

	int i = 0;
	for (auto& [index, _] : m_data.images) {
		if (m_data.swapchainImageRatio.contains(index)) continue;
		m_images[index] = images[i];
		m_resourceManager.setName(m_data.resourceNames[index], images[i]);

		i++;
	}

	i = 0;
	auto& buffers = m_resourceManager.getBuffers(m_frameDataAllocation);
	for (auto& [buffer, _] : m_data.buffers) {
		m_buffers[buffer] = buffers[i];
		m_resourceManager.setName(m_data.resourceNames[buffer], buffers[i]);

		i++;
	}

	buildSwapchainResources();
}
