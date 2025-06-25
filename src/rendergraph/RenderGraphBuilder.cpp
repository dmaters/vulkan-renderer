#include "RenderGraphBuilder.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <optional>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"

void RenderGraphBuilder::addTask(std::unique_ptr<Task> task) {
	std::vector<ImageDependencyInfo> images;
	std::vector<BufferDependencyInfo> buffers;
	task->setup(images, buffers);

	m_tasks.push_back({
		.task = std::move(task),
		.images = images,
		.buffers = buffers,
	});

	for (auto& image : images) {
		if (!m_imageReferences.contains(image.name))
			m_imageReferences[image.name] = std::vector<ResourceReference>();

		m_imageReferences[image.name].push_back({
			.task = (uint32_t)m_tasks.size() - 1,
			.usage = image.usage,
			.requiredLayout = image.requiredLayout,
		});
	}

	for (auto& buffer : buffers) {
		if (!m_bufferReferences.contains(buffer.name))
			m_bufferReferences[buffer.name] = std::vector<ResourceReference>();

		m_bufferReferences[buffer.name].push_back({
			.task = (uint32_t)m_tasks.size() - 1,
			.usage = buffer.usage,
		});
	}
}

std::vector<std::string_view> RenderGraphBuilder::getReferencedResources(
) const {
	std::vector<std::string_view> resources;
	for (auto& [name, _] : m_imageReferences) {
		resources.push_back(name);
	}
	for (auto& [name, _] : m_bufferReferences) {
		resources.push_back(name);
	}
	return resources;
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

GraphData RenderGraphBuilder::build() {
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
	std::set<TaskIndex> visitedTasks;
	std::queue<TaskIndex> tasksToVisit;

	for (auto reference : m_imageReferences["main_color"])
		tasksToVisit.push(reference.task);

	while (!tasksToVisit.empty()) {
		TaskIndex taskName = tasksToVisit.front();
		RegisteredTask& task = m_tasks[taskName];
		tasksToVisit.pop();
		if (visitedTasks.contains(taskName)) continue;

		std::unordered_map<std::string_view, vk::ImageMemoryBarrier2>
			imageBarriers;
		std::unordered_map<std::string_view, vk::BufferMemoryBarrier2>
			bufferBarriers;

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

		tasks.push_back(TaskData {
			.task = std::move(task.task),
			.barriers = {
						 .imageBarriers = imageBarriers,
						 .bufferBarriers = bufferBarriers,
						 }
        });
	}
	std::reverse(tasks.begin(), tasks.end());
	std::unordered_map<std::string_view, ImageDependencyInfo> requiredLayouts;

	for (auto& [name, references] : m_imageReferences) {
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
