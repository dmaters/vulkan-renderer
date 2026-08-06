#include "RenderGraphRunner.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "BuildContext.hpp"
#include "DataProvider.hpp"
#include "Instance.hpp"
#include "Swapchain.hpp"
#include "Task.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/GraphData.hpp"
#include "resources/ResourceManager.hpp"
#include "ui/UI.hpp"

using namespace rendergraph::internal;
struct ResourceAllocationData {
	std::vector<ResourceManager::ImageDescription> imageDescriptions;
	std::vector<rendergraph::ResourceIndex> imagesIndices;

	std::vector<ResourceManager::BufferDescription> bufferDescriptions;
	std::vector<rendergraph::ResourceIndex> bufferIndices;
};
std::optional<ResourceManager::AllocationIndex> mapResources(
	ResourceManager& resourceManager,
	ResourceAllocationData& allocationData,
	ResourceManager::MemoryLocation location,
	std::unordered_map<rendergraph::ResourceIndex, ImageHandle>& imageMap,
	std::unordered_map<rendergraph::ResourceIndex, BufferHandle>& bufferMap,
	std::unordered_map<rendergraph::ResourceIndex, std::string>& names
) {
	if (allocationData.imageDescriptions.size() == 0 &&
	    allocationData.bufferDescriptions.size() == 0)
		return std::nullopt;

	ResourceManager::AllocationIndex allocation =
		resourceManager.createResources(
			allocationData.imageDescriptions,
			allocationData.bufferDescriptions,
			location
		);

	auto& imageHandles = resourceManager.getImages(allocation);

	for (int i = 0; i < imageHandles.size(); i++) {
		rendergraph::ResourceIndex index = allocationData.imagesIndices[i];
		imageMap[index] = imageHandles[i];
		resourceManager.setName(names[index], imageHandles[i]);
	}

	auto& bufferHandles = resourceManager.getBuffers(allocation);

	for (int i = 0; i < bufferHandles.size(); i++) {
		rendergraph::ResourceIndex index = allocationData.bufferIndices[i];
		bufferMap[index] = bufferHandles[i];
		resourceManager.setName(names[index], bufferHandles[i]);
	}

	return allocation;
}

struct BuildData {
	std::unordered_map<rendergraph::ResourceIndex, ImageHandle> imageMap;
	std::unordered_map<rendergraph::ResourceIndex, BufferHandle> bufferMap;
	std::optional<ResourceManager::AllocationIndex> deviceAllocation;
	std::optional<ResourceManager::AllocationIndex> sharedAllocation;
	std::optional<ResourceManager::AllocationIndex> hostAllocation;
};
BuildData build(ExecutionInfo& execInfo, ResourceManager& resourceManager) {
	ResourceAllocationData deviceAllocationData;
	ResourceAllocationData sharedAllocationData;
	ResourceAllocationData hostAllocationData;

	for (auto [index, data] : execInfo.resources.images) {
		ResourceAllocationData* currentdata;
		switch (data.location) {
			case ResourceManager::MemoryLocation::Device:
				currentdata = &deviceAllocationData;
				break;
			case ResourceManager::MemoryLocation::HostVisible:
				currentdata = &sharedAllocationData;
				break;
			case ResourceManager::MemoryLocation::Host:
				currentdata = &hostAllocationData;
				break;
		}

		currentdata->imageDescriptions.push_back(data.description);
		currentdata->imagesIndices.push_back(index);
	}
	for (auto [index, data] : execInfo.resources.buffers) {
		ResourceAllocationData* currentdata;
		switch (data.location) {
			case ResourceManager::MemoryLocation::Device:
				currentdata = &deviceAllocationData;
				break;
			case ResourceManager::MemoryLocation::HostVisible:
				currentdata = &sharedAllocationData;
				break;
			case ResourceManager::MemoryLocation::Host:
				currentdata = &hostAllocationData;
				break;
		}

		currentdata->bufferDescriptions.push_back(data.description);
		currentdata->bufferIndices.push_back(index);
	}

	// TODO: Handle indexing better
	std::unordered_map<rendergraph::ResourceIndex, ImageHandle> imageMap;
	std::unordered_map<rendergraph::ResourceIndex, BufferHandle> bufferMap;

	auto deviceAllocation = mapResources(
		resourceManager,
		deviceAllocationData,
		ResourceManager::MemoryLocation::Device,
		imageMap,
		bufferMap,
		execInfo.resources.names
	);
	auto sharedAllocation = mapResources(
		resourceManager,
		sharedAllocationData,
		ResourceManager::MemoryLocation::HostVisible,
		imageMap,
		bufferMap,
		execInfo.resources.names
	);
	auto hostAllocation = mapResources(
		resourceManager,
		hostAllocationData,
		ResourceManager::MemoryLocation::Host,
		imageMap,
		bufferMap,
		execInfo.resources.names
	);

	return {
		.imageMap = imageMap,
		.bufferMap = bufferMap,
		.deviceAllocation = deviceAllocation,
		.sharedAllocation = sharedAllocation,
		.hostAllocation = hostAllocation,
	};
}

RenderGraphRunner::RenderGraphRunner(
	GraphData& graphData,
	Swapchain& swapchain,
	ResourceManager& resourceManager,
	MaterialManager& materialManager,
	ExecutionInfo execInfo
) :
	m_data(graphData),
	m_swapchain(swapchain),
	m_resourceManager(resourceManager),
	m_materialManager(materialManager) {
	vk::Device& device = Instance::Get().device;
	m_renderSemaphores = {
		device.createSemaphore({}),
		device.createSemaphore({}),
		device.createSemaphore({}),
	};

	m_execInfo = execInfo;
	m_initialized = false;

	BuildData data = build(execInfo, m_resourceManager);

	m_deviceAllocation = data.deviceAllocation;
	m_sharedAllocation = data.sharedAllocation;
	m_deviceAllocation = data.deviceAllocation;

	m_images = data.imageMap;
	m_buffers = data.bufferMap;
	for (auto& [index, handle] : graphData.externalImages) {
		m_images[index] = handle;
	}

	for (auto& [index, handle] : graphData.externalBuffers) {
		m_buffers[index] = handle;
	}
	// Finish initialization
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
RenderGraphRunner::~RenderGraphRunner() {
	Instance::Get().device.waitIdle();

	if (m_deviceAllocation.has_value())
		m_resourceManager.freeAllocation(*m_deviceAllocation);

	if (m_sharedAllocation.has_value())
		m_resourceManager.freeAllocation(*m_sharedAllocation);

	if (m_hostAllocation.has_value())
		m_resourceManager.freeAllocation(*m_hostAllocation);
}
void writeBarrier(
	vk::CommandBuffer commandBuffer,
	const std::span<ExecutionInfo::Barrier>& barriers,
	const std::unordered_map<rendergraph::ResourceIndex, ImageHandle>& images,
	const std::unordered_map<rendergraph::ResourceIndex, BufferHandle>& buffers,
	const ResourceManager& resourceManager
) {
	if (barriers.size() == 0) return;

	std::vector<vk::ImageMemoryBarrier2> imageBarriers;
	imageBarriers.reserve(barriers.size());
	std::vector<vk::BufferMemoryBarrier2> bufferBarriers;
	bufferBarriers.reserve(barriers.size());

	for (auto& barrier : barriers) {
		ResourceUsage::Access previousAccess =
			ResourceUsage::GetAccess(barrier.previousUsage);

		ResourceUsage::Access currentAccess =
			ResourceUsage::GetAccess(barrier.currentUsage);

		if (ResourceIndexer::ResourceIndex_getType(barrier.index) ==
		    ResourceIndexer::ResourceType::Image) {
			const Image& image =
				resourceManager.getImage(images.at(barrier.index));

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
										 .levelCount = image.mipLevels,
										 .layerCount = 1,
					},
                }
			);
		} else {
			const Buffer& buffer =
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
				.taskFrameTime[m_data.taskMetadata[taskIndex].name];

		UI::Data.performanceData
			.taskFrameTime[m_data.taskMetadata[taskIndex].name] =
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
void RenderGraphRunner::outputToSwapchain(
	vk::CommandBuffer& commandBuffer, uint32_t imageIndex
) const {
	ResourceIndex finalImage = m_execInfo.outputImage;

	rendergraph::internal::ExecutionInfo::Barrier firstBarrier[] = {
		{
         .previousUsage = ResourceUsage::Type::ColorAttachmentWrite,
         .currentUsage = ResourceUsage::Type::TransferSrc,
         .index = finalImage,
		 }
	};

	writeBarrier(
		commandBuffer,
		std::span<ExecutionInfo::Barrier>(firstBarrier),
		m_images,
		m_buffers,
		m_resourceManager
	);

	Image& swapchainImage = m_swapchain.getImage(imageIndex);
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
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &swapchainBarrier,
		}
	);

	Image& resultImage = m_resourceManager.getImage(m_images.at(finalImage));

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

	ExecutionInfo::Barrier secondBarrier[] = {
		{
         .previousUsage = ResourceUsage::Type::TransferSrc,
         .currentUsage = ResourceUsage::Type::ColorAttachmentWrite,
         .index = finalImage,
		 }
	};

	writeBarrier(
		commandBuffer,
		std::span<ExecutionInfo::Barrier>(secondBarrier),
		m_images,
		m_buffers,
		m_resourceManager
	);

	swapchainBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	swapchainBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
	swapchainBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
	swapchainBarrier.dstAccessMask = vk::AccessFlagBits2::eNone;
	swapchainBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
	swapchainBarrier.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;

	commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &swapchainBarrier,
		}
	);
}

bool RenderGraphRunner::submit(const Scene& scene) {
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
		if (result == vk::Result::eSuboptimalKHR) return false;

	} catch (vk::OutOfDateKHRError _) {
		return false;
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

	Task::DataProvider provider(m_data.taskData);

	for (int i = 0; i < m_execInfo.tasks.size(); i++) {
		TaskIndex taskIndex = m_execInfo.tasks[i];
		GraphData::TaskMetaData taskData = m_data.taskMetadata[taskIndex];

		Task::BuildContext context {
			.task = taskIndex,
			.commandBuffer = commandBuffer,
			.dataProvider = provider,
			.currentFrame = m_currentFrame,
			.images = m_images,
			.buffers = m_buffers,
			.inputs = m_execInfo.references.inputs[taskIndex],
			.outputs = m_execInfo.references.outputs[taskIndex],
			.resourceManager = m_resourceManager,
			.materialManager = m_materialManager,
			.scene = scene,
		};

		writeBarrier(
			commandBuffer,
			std::span<ExecutionInfo::Barrier>(m_execInfo.barriers[taskIndex]),
			m_images,
			m_buffers,
			m_resourceManager
		);

		commandBuffer.beginDebugUtilsLabelEXT(
			{
				.pLabelName = m_data.taskMetadata[taskIndex].name.data(),
			}
		);
		if (timingFetched)
			commandBuffer.writeTimestamp(
				vk::PipelineStageFlagBits::eTopOfPipe,
				m_debugQueryPool,
				i * 2 + timestampsCount * currentFrameInFlight + 2
			);

		m_data.tasks[taskIndex].build(context);

		if (timingFetched)
			commandBuffer.writeTimestamp(
				vk::PipelineStageFlagBits::eBottomOfPipe,
				m_debugQueryPool,
				i * 2 + timestampsCount * currentFrameInFlight + 1 + 2
			);
		commandBuffer.endDebugUtilsLabelEXT();
	}

	outputToSwapchain(commandBuffer, imageIndex);

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
	auto [resourceSemaphore, expectedValue] =
		m_resourceManager.getSemaphoreInfo();
	uint64_t semaphoreWaitValues[2] = { expectedValue, 0 };

	vk::Semaphore semaphores[2] = {
		resourceSemaphore,
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
		if (presentResult == vk::Result::eSuboptimalKHR) return false;

	} catch (vk::OutOfDateKHRError _) {
		return false;
	}

	m_currentFrame++;
	return true;
}
