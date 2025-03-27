#include "RenderGraph.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <optional>
#include <queue>
#include <stack>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "RenderGraphResourceSolver.hpp"
#include "Swapchain.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphResourceSolver.hpp"
#include "resources/Buffer.hpp"
#include "resources/Image.hpp"
#include "resources/ResourceManager.hpp"

RenderGraph::RenderGraph(
	Instance& instance, Swapchain& swapchain, ResourceManager& resourceManager
) :
	m_instance(instance),
	m_swapchain(swapchain),
	m_resourceManager(resourceManager) {
	m_mainQueue = m_instance.device.getQueue(
		m_instance.queueFamiliesIndices.graphicsIndex, 0
	);

	m_swapchainImages = {
		m_resourceManager.registerImage(m_swapchain.getImage(0)),
		m_resourceManager.registerImage(m_swapchain.getImage(1)),
		m_resourceManager.registerImage(m_swapchain.getImage(2)),
	};

	m_resourceManager.setName("result", m_swapchainImages[0]);
}

bool isWriteOperation(vk::AccessFlags2 flags) {
	return flags & vk::AccessFlagBits2::eColorAttachmentWrite ||
	       flags & vk::AccessFlagBits2::eTransferWrite;
}

bool isBarrierNeeded(vk::AccessFlags2 current, vk::AccessFlags2 operation) {
	return isWriteOperation(current) ^ isWriteOperation(operation);
}

bool RenderGraph::addImageBarrier(
	RenderGraphResourceSolver::ImageDependencyInfo& imageReference,
	vk::ImageMemoryBarrier2& imageBarrier
) {
	Image& image = m_resourceManager.getNamedImage(imageReference.name);

	uint8_t accessIndex = image.transient ? m_currentFrame : 0;

	vk::AccessFlags2 currentAccess = image.accesses[accessIndex].accessType;
	vk::PipelineStageFlags2 currentStage =
		image.accesses[accessIndex].accessStage;

	image.accesses[accessIndex].accessType = imageReference.usage.access;
	image.accesses[accessIndex].accessStage = imageReference.usage.stage;

	if (!isBarrierNeeded(currentAccess, imageReference.usage.access) ||
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

	commandBuffer.pipelineBarrier2(vk::DependencyInfo {
		.imageMemoryBarrierCount = (uint32_t)imageBarriers.size(),
		.pImageMemoryBarriers = imageBarriers.data(),

	});

	// Final barriers
	std::vector<vk::ImageMemoryBarrier2> finalImageBarriers;
	for (auto& imageReference : task.images) {
		Image& image = m_resourceManager.getNamedImage(imageReference.name);
		uint8_t accessIndex = image.transient ? m_currentFrame : 0;

		vk::ImageMemoryBarrier2 barrier;
		if (addImageBarrier(imageReference, barrier))
			finalImageBarriers.push_back(barrier);
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

		if (!isBarrierNeeded(currentAccess, bufferReference.usage.access))
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
	if (finalImageBarriers.size() == 0 && bufferBarriers.size() == 0) return;

	commandBuffer.pipelineBarrier2(vk::DependencyInfo {
		.bufferMemoryBarrierCount = (uint32_t)bufferBarriers.size(),
		.pBufferMemoryBarriers = bufferBarriers.data(),
		.imageMemoryBarrierCount = (uint32_t)finalImageBarriers.size(),
		.pImageMemoryBarriers = finalImageBarriers.data(),

	});
}

void RenderGraph::submit(const std::vector<Primitive>& primitives) {
	const Frame& frame = m_swapchain.getNextFrame();

	auto _ = m_instance.device.waitForFences({ frame.fence }, vk::True, 1000);
	m_instance.device.resetFences(frame.fence);

	vk::AcquireNextImageInfoKHR acquireInfo;
	acquireInfo.swapchain = m_swapchain.getSwapchain();
	acquireInfo.timeout = 1500;
	acquireInfo.semaphore = frame.imageAvailable;
	acquireInfo.fence = nullptr;
	acquireInfo.deviceMask = 1;

	auto nextImageRes = m_instance.device.acquireNextImage2KHR(acquireInfo);
	if (nextImageRes.result != vk::Result::eSuccess) throw;

	uint32_t imageIndex = nextImageRes.value;

	m_resourceManager.setName("result", m_swapchainImages[imageIndex]);

	vk::CommandBufferAllocateInfo commandInfo;

	commandInfo.commandBufferCount = 1;
	commandInfo.commandPool = frame.commandPool;
	commandInfo.level = vk::CommandBufferLevel::ePrimary;

	auto commandBuffer =
		m_instance.device.allocateCommandBuffers(commandInfo)[0];
	commandBuffer.begin(vk::CommandBufferBeginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
	});
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

	vk::ImageMemoryBarrier2 presentBarrier;
	RenderGraphResourceSolver::ImageDependencyInfo presentDependency {
		.name = "result",
		.usage = {
			.type = ResourceUsage::Type::READ,
			.access = vk::AccessFlagBits2::eNone,
			.stage = vk::PipelineStageFlagBits2::eNone,
		},
		.requiredLayout = vk::ImageLayout::ePresentSrcKHR,
	};
	addImageBarrier(presentDependency, presentBarrier);

	commandBuffer.pipelineBarrier2(vk::DependencyInfo {
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &presentBarrier,
	});

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

	m_mainQueue.submit({ submitInfo }, { frame.fence });

	vk::PresentInfoKHR presentInfo {};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.renderFinished;

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = { &(m_swapchain.getSwapchain()) };

	presentInfo.pImageIndices = &imageIndex;

	m_mainQueue.presentKHR(presentInfo);

	m_currentFrame = (m_currentFrame + 1) % 3;
}

void RenderGraph::buildGraph() {
	std::set<std::string_view> visitedTasks;
	std::queue<std::string_view> tasksToVisit;
	m_nodes.clear();
	for (auto reference : m_imageReferences["result"])
		tasksToVisit.push(reference);

	while (!tasksToVisit.empty()) {
		std::string_view taskName = tasksToVisit.front();
		RegisteredTask& task = m_registeredTask[taskName];
		tasksToVisit.pop();

		if (visitedTasks.contains(taskName)) continue;

		m_nodes.push_back(taskName);
		visitedTasks.insert(taskName);
		for (auto& dependency : task.images) {
			for (auto& task : m_imageReferences[dependency.name]) {
				tasksToVisit.push(task);
			}
		}
		for (auto& dependency : task.buffers) {
			for (auto& task : m_bufferReferences[dependency.name])
				tasksToVisit.push(task);
		}
	}
	std::reverse(m_nodes.begin(), m_nodes.end());
}

void RenderGraph::addTask(std::string_view name, std::unique_ptr<Task> task) {
	RenderGraphResourceSolver solver;

	task->setup(solver);

	for (auto& imageDependency : solver.m_imageDependencies) {
		m_resourceManager.getNamedImage(imageDependency.name);

		if (imageDependency.usage.type == ResourceUsage::Type::WRITE)
			m_imageReferences[imageDependency.name].push_back(name);
	}

	for (auto& bufferDependency : solver.m_bufferDependencies) {
		m_resourceManager.getNamedBuffer(bufferDependency.name);

		if (bufferDependency.usage.type == ResourceUsage::Type::WRITE)
			m_bufferReferences[bufferDependency.name].push_back(name);
	}

	m_registeredTask[name] = {
		.task = std::move(task),
		.images = solver.m_imageDependencies,
		.buffers = solver.m_bufferDependencies,
	};

	buildGraph();
}