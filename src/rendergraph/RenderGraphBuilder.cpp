#include "RenderGraphBuilder.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"

void RenderGraphBuilder::addTask(std::string_view name, Task& task) {
	std::vector<ImageDependencyInfo> images;
	std::vector<BufferDependencyInfo> buffers;
	task.setup(images, buffers);

	assert(!m_tasks.contains(name));

	m_tasks[name] = { .name = name, .images = images, .buffers = buffers };

	for (auto& image : images) {
		if (!m_imageReferences.contains(image.name))
			m_imageReferences[image.name] = std::vector<ResourceReference>();

		m_imageReferences[image.name].push_back({
			.task = name,
			.usage = image.usage,
			.requiredLayout = image.requiredLayout,
		});
	}

	for (auto& buffer : buffers) {
		if (!m_bufferReferences.contains(buffer.name))
			m_bufferReferences[buffer.name] = std::vector<ResourceReference>();

		m_bufferReferences[buffer.name].push_back({
			.task = name,
			.usage = buffer.usage,
		});
	}
}

bool isBarrierNeeded(
	const ResourceUsage& previousUsage, const ResourceUsage& currentUsage
) {
	if (currentUsage.type == ResourceUsage::Type::WRITE) return false;
	if (currentUsage.type == ResourceUsage::Type::READ &&
	    previousUsage.type == ResourceUsage::Type::READ)
		return false;

	return true;
}
bool buildBufferBarrier(
	const ResourceUsage& previousUsage,
	const ResourceUsage& currentUsage,
	vk::BufferMemoryBarrier2& barrier
) {
	bool barrierNeeded = isBarrierNeeded(previousUsage, currentUsage);

	if (!barrierNeeded) return false;

	barrier = {
		.srcStageMask = previousUsage.stage,
		.srcAccessMask = previousUsage.access,
		.dstStageMask = currentUsage.stage,
		.dstAccessMask = currentUsage.access,
	};
	return true;
}
bool buildImageBarrier(
	const ResourceUsage& previousUsage,
	const ResourceUsage& currentUsage,
	const std::optional<vk::ImageLayout> previousLayout,
	const std::optional<vk::ImageLayout> currentLayout,
	vk::ImageMemoryBarrier2& barrier
) {
	bool barrierNeeded = isBarrierNeeded(previousUsage, currentUsage);

	if (!barrierNeeded &&
	    (previousLayout == currentLayout || !currentLayout.has_value()))
		return false;

	barrier = {
		.srcStageMask = previousUsage.stage,
		.srcAccessMask = previousUsage.access,
		.dstStageMask = currentUsage.stage,
		.dstAccessMask = currentUsage.access,
		.oldLayout = previousLayout.value_or(vk::ImageLayout::eUndefined),
		.newLayout = currentLayout.value_or(vk::ImageLayout::eUndefined)
	};
	return true;
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
GraphData RenderGraphBuilder::build(
	const std::set<std::string_view>& internalResources,
	ResourceManager& resourceManager
) {
	const auto sort = [](const ResourceReference& r1,
	                     const ResourceReference& r2) {
		return r1.usage.type < r2.usage.type;
	};

	for (auto& [name, accesses] : m_imageReferences) {
		std::sort(accesses.begin(), accesses.end(), sort);
	}

	for (auto& [name, accesses] : m_bufferReferences) {
		std::sort(accesses.begin(), accesses.end(), sort);
	}

	std::vector<TaskData> tasks;
	std::set<std::string_view> visitedTasks;
	std::queue<std::string_view> tasksToVisit;

	for (auto reference : m_imageReferences["result"])
		tasksToVisit.push(reference.task);

	while (!tasksToVisit.empty()) {
		std::string_view taskName = tasksToVisit.front();
		RegisteredTask& task = m_tasks[taskName];
		tasksToVisit.pop();
		if (visitedTasks.contains(taskName)) continue;

		std::unordered_map<std::string_view, vk::ImageMemoryBarrier2>
			imageBarriers;
		std::unordered_map<std::string_view, vk::BufferMemoryBarrier2>
			bufferBarriers;

		std::vector<ImageDependencyInfo> images;
		std::vector<BufferDependencyInfo> externalBuffers;

		visitedTasks.insert(taskName);
		for (auto& image : task.images) {
			auto& taskResourceAccesses = m_imageReferences[image.name];

			int accessIndex = 0;
			for (int i = 0; i < taskResourceAccesses.size(); i++) {
				accessIndex = i;
				auto& taskAccess = taskResourceAccesses[i];
				tasksToVisit.push(taskAccess.task);
				if (taskAccess.task == taskName) {
					break;
				}
			}

			images.push_back(image);
			if (!internalResources.contains(image.name)) continue;

			vk::ImageMemoryBarrier2 barrier;
			if (accessIndex > 0) {
				if (buildImageBarrier(
						taskResourceAccesses[accessIndex - 1].usage,
						taskResourceAccesses[accessIndex].usage,
						taskResourceAccesses[accessIndex - 1].requiredLayout,
						taskResourceAccesses[accessIndex].requiredLayout,
						barrier
					)) {
					imageBarriers[image.name] = barrier;
				}
			} else {
				if (buildImageBarrier(
						taskResourceAccesses.back().usage,
						taskResourceAccesses[0].usage,
						taskResourceAccesses.back().requiredLayout,
						taskResourceAccesses[0].requiredLayout,
						barrier
					)) {
					imageBarriers[image.name] = barrier;
				}
			}
		}
		for (auto& buffer : task.buffers) {
			auto& taskResourceAccesses = m_bufferReferences[buffer.name];

			int accessIndex = 0;
			for (int i = 0; i < taskResourceAccesses.size(); i++) {
				accessIndex = i;
				auto& taskAccess = taskResourceAccesses[i];
				tasksToVisit.push(taskAccess.task);
				if (taskAccess.task == taskName) {
					break;
				}
			}

			externalBuffers.push_back(buffer);

			if (!internalResources.contains(buffer.name)) continue;

			vk::BufferMemoryBarrier2 barrier;

			if (accessIndex > 0) {
				if (buildBufferBarrier(
						taskResourceAccesses[accessIndex - 1].usage,
						taskResourceAccesses[accessIndex].usage,
						barrier
					)) {
					bufferBarriers[buffer.name] = barrier;
				}
			} else {
				if (buildBufferBarrier(
						taskResourceAccesses.back().usage,
						taskResourceAccesses[0].usage,
						barrier
					)) {
					bufferBarriers[buffer.name] = barrier;
				}
			}
		}

		tasks.push_back({
			.name = taskName,
			.barrier =
				buildBarrier(imageBarriers, bufferBarriers, resourceManager),
			.requiredImages = images,
			.requiredBuffers = externalBuffers,
		});
	}
	std::reverse(tasks.begin(), tasks.end());
	std::unordered_map<std::string_view, ImageDependencyInfo> requiredLayouts;

	for (auto& [name, references] : m_imageReferences) {
		if (!internalResources.contains(name)) continue;
		auto it = std::find_if(
			references.rbegin(),
			references.rend(),
			[](const ResourceReference& image) {
				return image.requiredLayout.has_value();
			}
		);
		if (it == references.rend()) continue;

		requiredLayouts[name] = {
			.name = name,
			.usage = it->usage,
			.requiredLayout = it->requiredLayout,
		};
	}
	return { tasks, requiredLayouts };
}
