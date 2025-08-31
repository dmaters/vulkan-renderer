#include "RenderGraphBuilder.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "resources/Image.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"

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
		.newLayout = currentLayout.value_or(vk::ImageLayout::eUndefined),
	};
	return true;
}

void RenderGraphBuilder::addTask(std::string_view name, std::vector<ResourceDependency> inputResources,std::vector<ResourceDependency> outputResources, Task task) {
	
	m_tasks.push_back({
		.task = task,
		.name = name,
		.images = inputResources,
		.buffers = outputResources,
	});

	for (const auto& resource : inputResources) {
		if (!m_imageReferences.contains(resource.index))
			m_imageReferences[resource.index] = std::vector<ResourceTaskReference>();

		m_imageReferences[resource.index].push_back({
			.task = (uint32_t)m_tasks.size() - 1,
			.usage = resource.usage,
			.requiredLayout = resource.requiredLayout,
		});
	}
	for (const auto& resource : outputResources) {
		if (!m_bufferReferences.contains(resource.index))
			m_bufferReferences[resource.index] = std::vector<ResourceTaskReference>();

		m_bufferReferences[resource.index].push_back({
			.task = (uint32_t)m_tasks.size() - 1,
			.usage = resource.usage,
		});
	}
}


GraphData::TaskData RenderGraphBuilder::visitTask(RegisteredTask& task) const {

	std::unordered_map<std::string_view, vk::ImageMemoryBarrier2>
		imageBarriers;
	std::unordered_map<std::string_view, vk::BufferMemoryBarrier2>
		bufferBarriers;

	for (auto& [index, reference] : task.resources) {
		
		auto& taskResourceAccesses = m_imageReferences[name];
		requiredImages.insert(name);
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
				imageBarriers[name] = barrier;
			}
		}
	}
	for (auto& [name, reference] : task.buffers) {
		auto& taskResourceAccesses = m_bufferReferences[name];
		requiredBuffers.insert(name);
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
				bufferBarriers[name] = barrier;
			}
		} else {
			if (buildBufferBarrier(
					taskResourceAccesses.back().usage,
					taskResourceAccesses[0].usage,
					barrier
				)) {
				bufferBarriers[name] = barrier;
			}
		}
	}

	return GraphData::TaskData {
		.task = std::move(task.task),
		.barriers = {
			 .imageBarriers = imageBarriers,
			 .bufferBarriers = bufferBarriers,
		}
    };
}


GraphData RenderGraphBuilder::build() {

	std::vector<GraphData::TaskData> tasks;
	
	for(auto& it = m_tasks.rbegin(); it != m_tasks.rend(); it++){
		auto& task = *it;

		tasks.push_back(GraphData::TaskData{
			.name = task.
		})

	}



	std::reverse(tasks.begin(), tasks.end());

	std::unordered_map<std::string_view, GraphData::ImageStatus> imageStatus;

	for (auto& [name, taskRef] : m_imageReferences) {
		auto find = [](RenderGraphBuilder::ResourceTaskReference& dependency) {
			return dependency.requiredLayout.has_value();
		};

		auto firstLayout = std::find_if(taskRef.begin(), taskRef.end(), find);
		auto lastLayout = std::find_if(taskRef.rbegin(), taskRef.rend(), find);

		if (firstLayout == taskRef.end()) continue;

		imageStatus[name] = {
			firstLayout->requiredLayout.value(),
			lastLayout->requiredLayout.value(),
		};
	}

	return {
		.tasks = tasks,
		.transientImagesRequired = requiredImages,
		.transientBuffersRequired = requiredBuffers,
		.imageStatuses = imageStatus,
	};
}
