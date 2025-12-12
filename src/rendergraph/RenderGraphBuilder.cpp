#include "RenderGraphBuilder.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "ResourceUsage.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/MemoryBarrier.hpp"
#include "tasks/Task.hpp"

ResourceIndex RenderGraphBuilder::createImage(
	std::string_view name,
	ResourceManager::ImageDescription desc,
	uint8_t swapchainRatio
) {
	ResourceIndex index = ++m_resourceCount;
	m_images[index] = desc;

	if (swapchainRatio != 0) m_swapchainImageRatio[index] = swapchainRatio;

	m_names[index] = name;

	return index;
}

ResourceIndex RenderGraphBuilder::createBuffer(
	std::string_view name, ResourceManager::BufferDescription desc
) {
	ResourceIndex index = ++m_resourceCount;
	m_buffers[index] = desc;
	m_names[index] = name;

	return index;
}

struct RenderGraphBuilder::ResourceAccess {
	struct Barrier {
		vk::PipelineStageFlags2 dstStageMask;
		vk::AccessFlags2 dstAccessMask;
		vk::ImageLayout newLayout;
		vk::PipelineStageFlags2 srcStageMask;
		vk::AccessFlags2 srcAccessMask;
		vk::ImageLayout oldLayout;
	};
	std::optional<Barrier> barrier;
	uint32_t task;
};

RenderGraphBuilder::ResourceAccess RenderGraphBuilder::getResourceBarrier(
	uint32_t taskIndex,
	ResourceIndex resourceIndex,
	std::unordered_set<FeatureIndex>& enabledFeatures,
	bool readOnly
) const {
	uint8_t taskCount = m_dependencies.at(resourceIndex).size();
	std::vector<TaskResourceDependency> resourceDependencies =
		m_dependencies.at(resourceIndex);

	int i;
	for (i = 0; i < taskCount; i++) {
		if (resourceDependencies[i].taskIndex != taskIndex) continue;
		break;
	}
	assert(i != taskCount);

	int usageIndex = i;
	i = (taskCount + i - 1) % taskCount;

	ResourceUsage::Type currentUsage = resourceDependencies[usageIndex].usage;
	auto [currentStage, currentAccess, currentLayout] =
		ResourceUsage::GetAccess(currentUsage);

	while (usageIndex != i) {
		FeatureIndex feature =
			m_taskFeatures[resourceDependencies[i].taskIndex];

		if (!enabledFeatures.contains(feature)) {
			i = (taskCount + i - 1) % taskCount;
			continue;
		}
		ResourceUsage::Type previousUsage = resourceDependencies[i].usage;
		auto [previousStage, previousAccess, previousLayout] =
			ResourceUsage::GetAccess(resourceDependencies[i].usage);
		if (readOnly && (currentUsage == previousUsage))
			return {
				.barrier = std::nullopt,
				.task = resourceDependencies[i].taskIndex,
			};  // Read after read

		return ResourceAccess {
			.barrier =
				ResourceAccess::Barrier {
										 .dstStageMask = currentStage,
										 .dstAccessMask = currentAccess,
										 .newLayout = currentLayout,
										 .srcStageMask = previousStage,
										 .srcAccessMask = previousAccess,
										 .oldLayout = previousLayout,
										 },
			.task = resourceDependencies[i].taskIndex,
		};
	}
	return { {}, taskIndex };
}

RenderGraphBuilder::Barrier RenderGraphBuilder::getBarrier(
	uint32_t taskIndex, std::unordered_set<FeatureIndex>& enabledFeatures
) const {
	std::unordered_map<ResourceIndex, vk::ImageMemoryBarrier2> imageBarriers;
	std::unordered_map<ResourceIndex, vk::BufferMemoryBarrier2> bufferBarriers;

	std::unordered_set<uint32_t> previousTasks;
	for (auto& [index, reference] : m_tasks.at(taskIndex).inputs) {
		auto access =
			getResourceBarrier(taskIndex, index, enabledFeatures, true);

		previousTasks.insert(access.task);
		if (!access.barrier.has_value()) continue;
		if (m_images.contains(index)) {
			imageBarriers[index] = {
				.srcStageMask = access.barrier->srcStageMask,
				.srcAccessMask = access.barrier->srcAccessMask,
				.dstStageMask = access.barrier->dstStageMask,
				.dstAccessMask = access.barrier->dstAccessMask,
				.oldLayout = access.barrier->oldLayout,
				.newLayout = access.barrier->newLayout,
			};
		} else {
			bufferBarriers[index] = {
				.srcStageMask = access.barrier->srcStageMask,
				.srcAccessMask = access.barrier->srcAccessMask,
				.dstStageMask = access.barrier->dstStageMask,
				.dstAccessMask = access.barrier->dstAccessMask,
			};
		}
	}

	for (auto& [index, reference] : m_tasks.at(taskIndex).outputs) {
		auto access =
			getResourceBarrier(taskIndex, index, enabledFeatures, true);

		previousTasks.insert(access.task);
		if (!access.barrier.has_value()) continue;
		if (m_images.contains(index)) {
			imageBarriers[index] = {
				.srcStageMask = access.barrier->srcStageMask,
				.srcAccessMask = access.barrier->srcAccessMask,
				.dstStageMask = access.barrier->dstStageMask,
				.dstAccessMask = access.barrier->dstAccessMask,
				.oldLayout = access.barrier->oldLayout,
				.newLayout = access.barrier->newLayout,
			};
		} else {
			bufferBarriers[index] = {
				.srcStageMask = access.barrier->srcStageMask,
				.srcAccessMask = access.barrier->srcAccessMask,
				.dstStageMask = access.barrier->dstStageMask,
				.dstAccessMask = access.barrier->dstAccessMask,
			};
		}
	}

	return {
		.barrierTask =
			(!imageBarriers.empty() || !bufferBarriers.empty())
				? std::optional<GraphData::TaskData>({
					  .task =
						  [imageBarriers = std::move(imageBarriers),
		                   bufferBarriers = std::move(bufferBarriers)](
							  TaskContext& context
						  ) {
							  MemoryBarrier(
								  context, bufferBarriers, imageBarriers
							  );
						  },
					  .type = TaskType::Graphic,
					  .name = "",
					  .inputs = {},
					  .outputs = {},
				  })
				: std::nullopt,
		.previousTasks = std::move(previousTasks),
	};
}

void RenderGraphBuilder::addTask(
	std::string_view name,
	TaskType type,
	std::vector<ResourceDependency> inputResources,
	std::vector<ResourceDependency> outputResources,
	Task task,
	FeatureIndex featureIndex
) {
	uint32_t taskIndex = m_tasks.size();

	m_tasks.push_back(
		{
			.task = std::move(task),
			.type = type,
			.name = name,
			.inputs = inputResources,
			.outputs = outputResources,
		}
	);
	m_taskFeatures.push_back(featureIndex);

	for (auto& [index, dependency] : inputResources) {
		if (!m_dependencies.contains(index))
			m_dependencies[index] = std::vector<TaskResourceDependency>();

		assert(!m_dependencies.empty()
		);  // We don't read uninitialized resource

		m_dependencies[index].push_back(
			TaskResourceDependency {
				.usage = dependency,
				.taskIndex = taskIndex,
				.usageType = TaskResourceDependency::UsageType::Input,
			}
		);
	}

	for (auto& [index, dependency] : outputResources) {
		if (!m_dependencies.contains(index))
			m_dependencies[index] = std::vector<TaskResourceDependency>();

		assert(
			m_dependencies[index].empty() ||
			(!m_dependencies[index].empty() &&
		     m_dependencies[index].back().usageType !=
		         TaskResourceDependency::UsageType::Input)
		);  // We don't read uninitialized resource;

		m_dependencies[index].push_back(
			TaskResourceDependency {
				.usage = dependency,
				.taskIndex = taskIndex,
				.usageType = TaskResourceDependency::UsageType::Output,
			}
		);
	}

	m_taskFeatures.push_back(featureIndex);
}

GraphData RenderGraphBuilder::getData() const {
	std::unordered_map<ResourceIndex, vk::ImageLayout> requiredInitialLayouts;

	GraphData data;

	for (const auto& [index, dependencies] : m_dependencies) {
		auto findFunc = [](const TaskResourceDependency& dependency) {
			return ResourceUsage::GetAccess(dependency.usage).layout !=
			       vk::ImageLayout::eUndefined;
		};

		auto lastLayout =
			std::find_if(dependencies.rbegin(), dependencies.rend(), findFunc);

		if (lastLayout == dependencies.rend()) continue;

		requiredInitialLayouts[index] =
			ResourceUsage::GetAccess(lastLayout->usage).layout;
	}

	return {
		.swapchainImageRatio = m_swapchainImageRatio,
		.images = m_images,
		.buffers = m_buffers,
		.names = m_names,
	};
}

std::vector<GraphData::TaskData> RenderGraphBuilder::getTasks(
	ResourceIndex outputImage, std::unordered_set<FeatureIndex>& enabledFeatures
) const {
	assert(m_dependencies.contains(outputImage));

	std::queue<uint32_t> taskQueue;
	std::unordered_set<uint32_t> visitedTasks;

	std::vector<GraphData::TaskData> tasks;
	for (auto& taskDependency : m_dependencies.at(outputImage)) {
		taskQueue.push(taskDependency.taskIndex);
	}
	while (!taskQueue.empty()) {
		uint32_t taskIndex = taskQueue.front();
		taskQueue.pop();

		if (visitedTasks.contains(taskIndex)) continue;
		if (!enabledFeatures.contains(m_taskFeatures[taskIndex])) continue;

		tasks.push_back(m_tasks[taskIndex]);

		auto [barrier, referencedTasks] =
			getBarrier(taskIndex, enabledFeatures);
		if (barrier.has_value()) tasks.push_back(barrier.value());

		for (uint32_t index : referencedTasks) {
			taskQueue.push(index);
		}
		visitedTasks.insert(taskIndex);
	}

	std::ranges::reverse(tasks.begin(), tasks.end());
	return tasks;
}
