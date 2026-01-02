#include "RenderGraphBuilder.hpp"

#include <algorithm>
#include <cassert>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "GraphData.hpp"
#include "ResourceUsage.hpp"
#include "tasks/MemoryBarrier.hpp"
#include "tasks/Task.hpp"

using namespace rendergraph::internal;

struct RenderGraphBuilder::Barrier {
	std::optional<GraphData::TaskData> taskData;
	std::optional<Task> task;
	std::unordered_set<uint32_t> previousTasks;
};

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

	auto [inputOffset, inputCount] = m_data.taskData[taskIndex].inputs;
	for (int i = inputOffset; i < inputOffset + inputCount; i++) {
		auto [index, dependency] = m_data.taskDependencies[i];

		auto access =
			getResourceBarrier(taskIndex, index, enabledFeatures, true);

		previousTasks.insert(access.task);
		if (!access.barrier.has_value()) continue;
		if (m_data.images.contains(index)) {
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
	auto [outputOffset, outputCount] = m_data.taskData[taskIndex].outputs;
	for (int i = outputOffset; i < outputOffset + outputCount; i++) {
		auto [index, dependency] = m_data.taskDependencies[i];

		auto access =
			getResourceBarrier(taskIndex, index, enabledFeatures, true);

		previousTasks.insert(access.task);
		if (!access.barrier.has_value()) continue;
		if (m_data.images.contains(index)) {
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
	bool isBarrier = (!imageBarriers.empty() || !bufferBarriers.empty());
	return {
		.taskData = isBarrier ? std::optional<GraphData::TaskData>({
									.type = TaskType::Graphic,
									.name = "",
									.inputs = {},
									.outputs = {},
								})
		                      : std::nullopt,
		.task = isBarrier ? std::optional<Task>([imageBarriers =
		                                             std::move(imageBarriers),
		                                         bufferBarriers = std::move(
													 bufferBarriers
												 )](TaskContext& context) {
			MemoryBarrier(context, bufferBarriers, imageBarriers);
		})
		                  : std::nullopt,

		.previousTasks = std::move(previousTasks),
	};
}

void RenderGraphBuilder::addTask(
	TaskIndex task,
	std::vector<ResourceDependency>& inputResources,
	std::vector<ResourceDependency>& outputResources,
	FeatureIndex featureIndex
) {
	m_taskFeatures.push_back(featureIndex);

	for (auto& [index, dependency] : inputResources) {
		if (!m_dependencies.contains(index))
			m_dependencies[index] = std::vector<TaskResourceDependency>();

		m_dependencies[index].push_back(
			TaskResourceDependency {
				.usage = dependency,
				.taskIndex = task,
				.usageType = TaskResourceDependency::UsageType::Input,
			}
		);
	}

	for (auto& [index, dependency] : outputResources) {
		if (!m_dependencies.contains(index))
			m_dependencies[index] = std::vector<TaskResourceDependency>();

		m_dependencies[index].push_back(
			TaskResourceDependency {
				.usage = dependency,
				.taskIndex = task,
				.usageType = TaskResourceDependency::UsageType::Output,
			}
		);
	}

	m_taskFeatures.push_back(featureIndex);
}

ExecutionInfo RenderGraphBuilder::getTasks(
	ResourceIndex outputImage, std::unordered_set<FeatureIndex>& enabledFeatures
) const {
	assert(m_dependencies.contains(outputImage));

	std::queue<uint32_t> taskQueue;
	std::unordered_set<uint32_t> visitedTasks;

	std::vector<TaskIndex> tasks;
	std::vector<GraphData::TaskData> barriersData;
	std::vector<Task> barrierTasks;

	uint32_t baseTaskOffset = m_data.tasks.size();

	for (auto& taskDependency : m_dependencies.at(outputImage)) {
		taskQueue.push(taskDependency.taskIndex);
	}
	while (!taskQueue.empty()) {
		uint32_t taskIndex = taskQueue.front();
		taskQueue.pop();

		if (visitedTasks.contains(taskIndex)) continue;
		if (!enabledFeatures.contains(m_taskFeatures[taskIndex])) continue;

		tasks.push_back(taskIndex);

		auto [taskData, task, referencedTasks] =
			getBarrier(taskIndex, enabledFeatures);
		if (taskData.has_value()) {
			tasks.push_back(baseTaskOffset + barrierTasks.size());
			barrierTasks.push_back(std::move(task.value()));
			barriersData.push_back(taskData.value());
		}

		for (uint32_t index : referencedTasks) {
			taskQueue.push(index);
		}
		visitedTasks.insert(taskIndex);
	}

	std::ranges::reverse(tasks.begin(), tasks.end());
	return {
		.tasks = std::move(tasks),
		.data = std::move(barriersData),
		.barriers = std::move(barrierTasks),
	};
}
