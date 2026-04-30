#include "RenderGraphRunner.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>
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
#include "tasks/TaskContext.hpp"
#include "ui/UI.hpp"

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
		m_resourceManager.freeDeviceAllocation(
			m_oldResolutionDependentAllocation
		);
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

void writeBarrier(
	vk::CommandBuffer commandBuffer,
	std::span<const ExecutionInfo::Barrier> barriers,
	std::unordered_map<ResourceIndex, ImageHandle>& images,
	std::unordered_map<ResourceIndex, BufferHandle>& buffers,
	ResourceManager& resourceManager
) {
	if (barriers.size() == 0) return;

	std::vector<vk::ImageMemoryBarrier2> imageBarriers;
	imageBarriers.reserve(barriers.size());
	std::vector<vk::BufferMemoryBarrier2> bufferBarriers;
	bufferBarriers.reserve(barriers.size());

	for (ExecutionInfo::Barrier barrier : barriers) {
		ResourceUsage::Access previousAccess =
			ResourceUsage::GetAccess(barrier.lastUsage);

		ResourceUsage::Access currentAccess =
			ResourceUsage::GetAccess(barrier.lastUsage);

		if (images.contains(barrier.index)) {
			Image& image = resourceManager.getImage(images.at(barrier.index));

			imageBarriers.push_back(
			    {
					.srcStageMask = previousAccess.stage,
					.srcAccessMask = previousAccess.access,
					.dstStageMask = currentAccess.stage,
					.dstAccessMask = currentAccess.access,
					.oldLayout = previousAccess.layout,
					.newLayout = currentAccess.layout,
					.image = image.image,
					.subresourceRange = {
										 .aspectMask = image.getAspectFlags(),
										 .levelCount = 1,
										 .layerCount = 1,
					},
                }
			);
		} else {
			Buffer& buffer =
				resourceManager.getBuffer(buffers.at(barrier.index));

			bufferBarriers.push_back(
				{
					.srcStageMask = previousAccess.stage,
					.srcAccessMask = previousAccess.access,
					.dstStageMask = currentAccess.stage,
					.dstAccessMask = currentAccess.access,
					.buffer = buffer.buffer,
					.offset = 0,
					.size = static_cast<uint32_t>(buffer.size),
				}
			);
		}

		commandBuffer.pipelineBarrier2(
			vk::DependencyInfo {
				.bufferMemoryBarrierCount =
					static_cast<uint32_t>(bufferBarriers.size()),
				.pBufferMemoryBarriers = bufferBarriers.data(),
				.imageMemoryBarrierCount =
					static_cast<uint32_t>(imageBarriers.size()),
				.pImageMemoryBarriers = imageBarriers.data(),

			}
		);
	}
}

void RenderGraphRunner::update(ResourceIndex output, ExecutionInfo execInfo) {
	m_output = output;
	m_execInfo = execInfo;
	m_initialized = false;

	vk::Device device = Instance::Get().device;

	uint32_t queryCount =
		static_cast<uint32_t>(execInfo.tasks.size() * 3 * 2 + 6);
	// TODO: Dispose of old query pool to reduce leaks
	m_debugQueryPool = device.createQueryPool(
		vk::QueryPoolCreateInfo {
			.queryType = vk::QueryType::eTimestamp,
			.queryCount = queryCount,
		}
	);
	device.resetQueryPool(m_debugQueryPool, 0, queryCount);
}

bool RenderGraphRunner::updateTimings() const {
	if (m_currentFrame < 3) return true;

	vk::Device device = Instance::Get().device;
	uint8_t currentFrameInFlight = m_currentFrame % 3;
	uint32_t timestampsCount = m_execInfo.tasks.size() * 2 + 2;

	std::vector<uint64_t> timestamps(timestampsCount);
	vk::Result result = device.getQueryPoolResults(
		m_debugQueryPool,
		timestampsCount * currentFrameInFlight,
		timestampsCount,
		sizeof(uint64_t) * timestampsCount,
		timestamps.data(),
		sizeof(uint64_t),
		vk::QueryResultFlagBits::e64
	);
	if (result != vk::Result::eSuccess) return false;

	device.resetQueryPool(
		m_debugQueryPool,
		timestampsCount * currentFrameInFlight,
		timestampsCount
	);

	uint32_t timestampOffset = 2;

	for (int i = 0; i < m_execInfo.tasks.size(); i++) {
		uint32_t taskIndex = m_execInfo.tasks[i];

		if (taskIndex >= m_data.tasks.size()) continue;  // It's a barrier

		float ms =
			static_cast<float>(
				timestamps[timestampOffset + 1] - timestamps[timestampOffset]
			) /
			10e6;

		float pastFrameTime =
			UI::Data.performanceData
				.taskFrameTime[m_data.taskData[taskIndex].name];

		UI::Data.performanceData
			.taskFrameTime[m_data.taskData[taskIndex].name] =
			(pastFrameTime / 64 * 63) + ms / 64;

		timestampOffset += 2;
	}

	float lastGpuFrameTime = UI::Data.performanceData.gpuFrameTime;
	float gpuMS = static_cast<float>(timestamps[1] - timestamps[0]) / 10e6;
	UI::Data.performanceData.gpuFrameTime =
		(lastGpuFrameTime / 64 * 63) + gpuMS / 64;

	UI::Data.performanceData.cpuFrameTime =
		(UI::Data.performanceData.cpuFrameTime / 64 * 63) +
		m_lastCpuFrameTime / 64;

	double lastTotalFrameTime =
		std::chrono::duration_cast<std::chrono::duration<double>>(
			std::chrono::steady_clock::now() - m_beginFrameTimestamp
		)
			.count();

	float currentFrameRate = lastTotalFrameTime > 0 ? (1.0 / lastTotalFrameTime)
	                                                : 0;
	UI::Data.performanceData.frameRate = static_cast<uint16_t>(
		UI::Data.performanceData.frameRate / 64.0 * 63.0 +
		currentFrameRate / 64.0
	);

	return true;
}

void RenderGraphRunner::submit(const Scene& scene) {
	clearUnusedResources();

	vk::Device device = Instance::Get().device;

	uint8_t currentFrameInFlight = m_currentFrame % 3;
	const Frame& frame = m_swapchain.getFrame(currentFrameInFlight);

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

	bool timingFetched =
		updateTimings();  // Get last frame timings and reset queryPool

	uint32_t timestampsCount = m_execInfo.tasks.size() * 2 + 2;

	// Begin Frame
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
	commandBuffer.writeTimestamp(
		vk::PipelineStageFlagBits::eTopOfPipe,
		m_debugQueryPool,
		timestampsCount * currentFrameInFlight
	);

	m_beginFrameTimestamp = std::chrono::steady_clock::now();

	if (!m_initialized) {
		m_initialized = true;

		writeBarrier(
			commandBuffer,
			m_execInfo.initializationBarriers,
			m_images,
			m_buffers,
			m_resourceManager
		);
	}

	for (int i = 0; i < m_execInfo.tasks.size(); i++) {
		uint32_t taskIndex = m_execInfo.tasks[i];
		GraphData::TaskData taskData = m_data.taskData[taskIndex];

		auto inputs = std::span<const ResourceDependency>(
			m_data.taskDependencies.data() + taskData.inputs.offset,
			m_data.taskDependencies.data() + taskData.inputs.offset +
				taskData.inputs.count
		);

		auto outputs = std::span<const ResourceDependency>(
			m_data.taskDependencies.data() + taskData.outputs.offset,
			m_data.taskDependencies.data() + taskData.outputs.offset +
				taskData.outputs.count
		);

		TaskContext context {
			.commandBuffer = commandBuffer,
			.inputs = inputs,
			.outputs = outputs,
			.images = m_images,
			.buffers = m_buffers,
			.currentFrame = m_currentFrame,
			.resourceManager = m_resourceManager,
			.materialManager = m_materialManager,
			.scene = scene,
		};

		uint32_t barrierOffset = m_execInfo.barrierOffsets[i * 2];
		uint32_t barrierCount =
			m_execInfo.barrierOffsets[i * 2 + 1] - barrierOffset;

		if (barrierCount > 0)
			writeBarrier(
				commandBuffer,
				std::span<const ExecutionInfo::Barrier>(
					m_execInfo.barriers.data() + barrierOffset, barrierCount
				),
				m_images,
				m_buffers,
				m_resourceManager
			);

		commandBuffer.beginDebugUtilsLabelEXT(
			{
				.pLabelName = m_data.taskData[taskIndex].name.data(),
			}
		);
		if (timingFetched)
			commandBuffer.writeTimestamp(
				vk::PipelineStageFlagBits::eTopOfPipe,
				m_debugQueryPool,
				i * 2 + timestampsCount * currentFrameInFlight
			);

		m_data.tasks[taskIndex](context);

		if (timingFetched)
			commandBuffer.writeTimestamp(
				vk::PipelineStageFlagBits::eBottomOfPipe,
				m_debugQueryPool,
				i * 2 + timestampsCount * currentFrameInFlight + 1
			);
		commandBuffer.endDebugUtilsLabelEXT();
	}

	outputToSwapchain(commandBuffer, m_output, imageIndex);
	commandBuffer.writeTimestamp(
		vk::PipelineStageFlagBits::eBottomOfPipe,
		m_debugQueryPool,
		timestampsCount * currentFrameInFlight + 1
	);

	m_lastCpuFrameTime =
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - m_beginFrameTimestamp
		)
			.count();

	commandBuffer.end();
	// End Frame

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

	m_currentFrame++;
}

void RenderGraphRunner::build() {
	std::vector<ResourceManager::ImageDescription> imageDescriptions;
	std::vector<ResourceIndex> imagesIndices;

	std::vector<ResourceManager::BufferDescription> bufferDescriptions;
	std::vector<ResourceIndex> bufferIndices;

	for (auto& [index, description] : m_data.images) {
		if (m_data.swapchainImageRatio.contains(index)) continue;
		if (m_data.externalImages.contains(index)) continue;

		imageDescriptions.push_back(description);
		imagesIndices.push_back(index);
	}
	for (auto& [index, description] : m_data.buffers) {
		if (m_data.externalBuffers.contains(index)) continue;
		if (m_data.sharedBuffers.contains(index)) continue;

		bufferDescriptions.push_back(description);
		bufferIndices.push_back(index);
	}

	m_frameDataAllocation = m_resourceManager.createResources(
		imageDescriptions, bufferDescriptions
	);
	auto& images = m_resourceManager.getImages(m_frameDataAllocation);

	for (int i = 0; i < imagesIndices.size(); i++) {
		m_images[imagesIndices[i]] = images[i];
	}

	auto& buffers = m_resourceManager.getBuffers(m_frameDataAllocation);

	for (int i = 0; i < bufferIndices.size(); i++) {
		m_buffers[bufferIndices[i]] = buffers[i];
		m_resourceManager.setName(
			m_data.resourceNames[bufferIndices[i]], buffers[i]
		);
	}

	if (m_data.sharedBuffers.size() > 0) {
		bufferDescriptions.clear();
		bufferIndices.clear();

		for (ResourceIndex index : m_data.sharedBuffers) {
			bufferDescriptions.push_back(m_data.buffers.at(index));
			bufferIndices.push_back(index);
		}

		m_sharedDataAllocation =
			m_resourceManager.createSharedAllocation(bufferDescriptions);

		auto buffers = m_resourceManager.getBuffers(m_sharedDataAllocation);
		for (int i = 0; i < bufferIndices.size(); i++) {
			m_buffers[bufferIndices[i]] = buffers[i];
			m_resourceManager.setName(
				m_data.resourceNames[bufferIndices[i]], buffers[i]
			);
		}
	}

	if (m_data.localBufferSizes.size() > 0) {
		std::vector<uint32_t> sizes;
		bufferIndices.clear();

		for (auto [index, size] : m_data.localBufferSizes) {
			sizes.push_back(size);
			bufferIndices.push_back(index);
		}

		// Return buffers so they can bound to resource index
		m_hostDataAllocation = m_resourceManager.createHostAllocation(sizes);

		auto buffers = m_resourceManager.getHostBuffers(*m_hostDataAllocation);
		for (int i = 0; i < bufferIndices.size(); i++) {
			m_buffers[bufferIndices[i]] = buffers[i];
			m_resourceManager.setName(
				m_data.resourceNames[bufferIndices[i]], buffers[i]
			);
		}
	}

	for (auto entry : m_data.externalImages) m_images.insert(entry);
	for (auto entry : m_data.externalBuffers) m_buffers.insert(entry);

	buildSwapchainResources();
}
