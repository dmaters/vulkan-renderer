#include "RenderGraph.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <queue>
#include <stack>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Primitive.hpp"
#include "RenderGraphResourceSolver.hpp"
#include "Swapchain.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphResourceSolver.hpp"
#include "rendergraph/tasks/ImageCopy.hpp"
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

RenderGraph::RenderGraph(Instance& instance, Swapchain& swapchain) :
	m_instance(instance), m_swapchain(swapchain), m_resourceManager(instance) {
	m_mainQueue = m_instance.device.getQueue(
		m_instance.queueFamiliesIndices.graphicsIndex, 0
	);
}

bool RenderGraph::addImageBarrier(
	RenderGraphResourceSolver::ImageDependencyInfo& imageReference,
	vk::ImageMemoryBarrier2& imageBarrier
) {
	bool isTransient = !m_images.contains(imageReference.name);

	ResourceUsage& currentUsage = m_resourceStatus[imageReference.name];
	Image& image = isTransient
	                   ? m_transientImages[imageReference.name][m_currentFrame]
	                   : m_images[imageReference.name];

	if (imageReference.usage == currentUsage &&
	    image.layout == imageReference.requiredLayout)
		return false;

	imageBarrier = vk::ImageMemoryBarrier2{
			.srcStageMask = currentUsage.stage,
			.srcAccessMask = currentUsage.access,
			.dstStageMask = imageReference.usage.stage,
			.dstAccessMask = imageReference.usage.access,
			.oldLayout = image.layout,
			.newLayout = imageReference.requiredLayout.value_or(image.layout),
			.image = image.image,
			.subresourceRange = {
							 .aspectMask =
		                             image.getAspectFlags(),
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1,
						},

		};
	image.layout = imageReference.requiredLayout.value_or(image.layout);
	m_resourceStatus[imageReference.name] = imageReference.usage;

	return true;
}

void RenderGraph::addMemoryBarriers(
	vk::CommandBuffer& commandBuffer, std::string_view taskName
) {
	RegisteredTask& task = m_registeredTask[taskName];

	std::vector<vk::ImageMemoryBarrier2> imageBarriers;
	std::vector<std::string_view> imagesToClear;
	for (auto imageReference : task.images) {
		if (!m_transientImages.contains(imageReference.name) ||
		    m_clearedImages.contains(imageReference.name))
			continue;
		vk::ImageMemoryBarrier2 barrier;
		imageReference.requiredLayout = vk::ImageLayout::eTransferDstOptimal;

		ResourceUsage usage {
			.type = ResourceUsage::Type::WRITE,
			.access = vk::AccessFlagBits2::eTransferWrite,
			.stage = vk::PipelineStageFlagBits2::eTransfer,
		};
		imageReference.usage = usage;
		m_resourceStatus[imageReference.name] = usage;

		if (addImageBarrier(imageReference, barrier)) {
			imageBarriers.push_back(barrier);
		}
		imagesToClear.push_back(imageReference.name);
		m_clearedImages.insert(imageReference.name);
	}

	commandBuffer.pipelineBarrier2(vk::DependencyInfo {
		.imageMemoryBarrierCount = (uint32_t)imageBarriers.size(),
		.pImageMemoryBarriers = imageBarriers.data(),

	});

	for (auto imageName : imagesToClear) {
		Image& image = m_transientImages[imageName][m_currentFrame];

		vk::ImageSubresourceRange range = {
			image.getAspectFlags(), 0, 1, 0, 1
		};

		if (image.getAspectFlags() == vk::ImageAspectFlagBits::eColor)
			commandBuffer.clearColorImage(
				image.image,
				image.layout,
				vk::ClearColorValue(std::array<float, 4> {
					0,
					0,
					0,
					0,
				}),
				{ range }

			);
		else
			commandBuffer.clearDepthStencilImage(
				image.image, image.layout, { 1, 0 }, { range }
			);
	}

	// Final barriers
	std::vector<vk::ImageMemoryBarrier2> finalImageBarriers;
	for (auto& imageReference : task.images) {
		vk::ImageMemoryBarrier2 barrier;
		if (addImageBarrier(imageReference, barrier))
			finalImageBarriers.push_back(barrier);
	}
	std::vector<vk::BufferMemoryBarrier2> bufferBarriers;

	for (auto& bufferReference : task.buffers) {
		ResourceUsage& currentUsage = m_resourceStatus[bufferReference.name];
		if (bufferReference.usage == currentUsage) continue;

		bool isTransient = !m_buffers.contains(bufferReference.name);
		Buffer& buffer =
			isTransient
				? m_transientBuffers[bufferReference.name][m_currentFrame]
				: m_buffers[bufferReference.name];

		bufferBarriers.push_back(vk::BufferMemoryBarrier2 {
			.srcStageMask = currentUsage.stage,
			.srcAccessMask = currentUsage.access,
			.dstStageMask = bufferReference.usage.stage,
			.dstAccessMask = bufferReference.usage.access,
			.buffer = buffer.buffer,
			.size = vk::WholeSize,
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
	/// Initialize command buffer;

	m_clearedImages.clear();

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

	m_images["result"] = m_swapchain.getImage(imageIndex);

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
		.images = m_images,
		.buffers = m_buffers,
		.transientImages = m_transientImages,
		.transientBuffers = m_transientBuffers,
		.primitives = primitives,
		.currentFrame = m_currentFrame,
	};

	for (auto& node : m_nodes) {
		RegisteredTask& task = m_registeredTask[node];
		addMemoryBarriers(commandBuffer, node);
		task.task->execute(commandBuffer, resources);
	}
	vk::ImageMemoryBarrier2 presentImageBarrier {
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.oldLayout = vk::ImageLayout::eTransferDstOptimal,
		.newLayout = vk::ImageLayout::ePresentSrcKHR,
		.image = m_images["result"].image,

		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	commandBuffer.pipelineBarrier2(vk::DependencyInfo {
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &presentImageBarrier,
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
		assert(
			imageDependency.name == "result" ||
			m_images.contains(imageDependency.name) ||
			m_transientImages.contains(imageDependency.name)
		);
		if (imageDependency.usage.type == ResourceUsage::Type::WRITE)
			m_imageReferences[imageDependency.name].push_back(name);
		m_resourceStatus[imageDependency.name] = imageDependency.usage;
	}

	for (auto& bufferDependency : solver.m_bufferDependencies) {
		assert(
			m_buffers.contains(bufferDependency.name) ||
			m_transientBuffers.contains(bufferDependency.name)
		);

		if (bufferDependency.usage.type == ResourceUsage::Type::WRITE)
			m_bufferReferences[bufferDependency.name].push_back(name);
		m_resourceStatus[bufferDependency.name] = bufferDependency.usage;
	}

	m_registeredTask[name] = {
		.task = std::move(task),
		.images = solver.m_imageDependencies,
		.buffers = solver.m_bufferDependencies,
	};

	buildGraph();
}
void RenderGraph::registerImage(std::string_view name, Image image) {
	m_images[name] = image;
}
void RenderGraph::registerImage(
	std::string_view name, const ResourceManager::ImageDescription& description
) {
	m_transientImages[name] = {
		m_resourceManager.createImage(description),
		m_resourceManager.createImage(description),
		m_resourceManager.createImage(description),
	};
}
void RenderGraph::registerBuffer(std::string_view name, Buffer buffer) {
	m_buffers[name] = buffer;
}
void RenderGraph::registerBuffer(
	std::string_view name, std::array<Buffer, 3> buffers
) {
	m_transientBuffers[name] = buffers;
}
void RenderGraph::registerBuffer(
	std::string_view name, const ResourceManager::BufferDescription& description
) {
	m_transientBuffers[name] = {
		m_resourceManager.createBuffer(description),
		m_resourceManager.createBuffer(description),
		m_resourceManager.createBuffer(description),

	};
}