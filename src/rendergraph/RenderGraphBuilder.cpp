#include "RenderGraphBuilder.hpp"

#include <cassert>
#include <optional>
#include <ranges>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "GraphData.hpp"
#include "ResourceUsage.hpp"
#include "tasks/Task.hpp"
#include "tasks/TaskContext.hpp"

using namespace rendergraph::internal;

struct RenderGraphBuilder::ResourceAccess {
	std::optional<ExecutionInfo::Barrier> barrier;
	uint32_t task;
	bool isTaskPreviousSubmission;
};

RenderGraphBuilder::ResourceAccess RenderGraphBuilder::getResourceBarrier(
	uint32_t taskIndex, ResourceIndex resourceIndex
) const {
	uint8_t taskCount = m_dependencies.at(resourceIndex).size();
	const std::vector<TaskResourceDependency>& resourceDependencies =
		m_dependencies.at(resourceIndex);

	int currentUsageIndex = -1;
	for (int i = 0; i < taskCount; i++) {
		if (resourceDependencies[i].taskIndex == taskIndex) {
			currentUsageIndex = i;
			break;
		}
	}
	assert(currentUsageIndex != -1);

	ResourceUsage::Type currentUsage =
		resourceDependencies[currentUsageIndex].usage;

	for (int i = 1; i <= taskCount; i++) {
		int previousUsageIndex =
			(taskCount + (currentUsageIndex - i)) % taskCount;

		ResourceUsage::Type previousUsage =
			resourceDependencies[previousUsageIndex].usage;

		if ((currentUsage == previousUsage) &&
		    ResourceUsage::IsReadAccess(currentUsage))
			continue;

		ExecutionInfo::Barrier barrier = {
			.lastUsage = previousUsage,
			.currentUsage = currentUsage,
			.index = resourceIndex,
		};

		return ResourceAccess {
			.barrier = barrier,
			.task = resourceDependencies[previousUsageIndex].taskIndex,
			.isTaskPreviousSubmission = previousUsageIndex >= currentUsageIndex,
		};
	}
	return {
		.barrier = std::nullopt,
		.task = resourceDependencies[currentUsageIndex].taskIndex,
		.isTaskPreviousSubmission = true,
	};
}

std::pair<std::vector<ExecutionInfo::Barrier>, std::unordered_set<uint32_t>>
RenderGraphBuilder::getBarriers(uint32_t taskIndex) const {
	std::vector<ExecutionInfo::Barrier> barriers;
	std::unordered_set<uint32_t> previousTasks;

	auto [inputOffset, inputCount] = m_data.taskData[taskIndex].inputs;
	for (int i = inputOffset; i < inputOffset + inputCount; i++) {
		auto [index, dependency] = m_data.taskDependencies[i];

		auto access = getResourceBarrier(taskIndex, index);

		if (!access.barrier.has_value()) continue;

		if (!access.isTaskPreviousSubmission && access.task != taskIndex)
			previousTasks.insert(access.task);
		barriers.push_back(*access.barrier);
	}
	auto [outputOffset, outputCount] = m_data.taskData[taskIndex].outputs;
	for (int i = outputOffset; i < outputOffset + outputCount; i++) {
		auto [index, dependency] = m_data.taskDependencies[i];

		auto access = getResourceBarrier(taskIndex, index);

		if (!access.barrier.has_value()) continue;

		if (!access.isTaskPreviousSubmission && access.task != taskIndex)
			previousTasks.insert(access.task);
		barriers.push_back(*access.barrier);
	}
	return {
		barriers,
		previousTasks,
	};
}

void RenderGraphBuilder::addTask(
	TaskIndex task,
	std::vector<ResourceDependency>& inputResources,
	std::vector<ResourceDependency>& outputResources
) {
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
}

ExecutionInfo RenderGraphBuilder::getTasks(ResourceIndex outputImage) const {
	assert(m_dependencies.contains(outputImage));

	std::stack<uint32_t> taskStack;
	std::unordered_set<uint32_t> visitedTasks;

	std::unordered_set<ResourceIndex> referencedImages;

	std::vector<TaskIndex> tasks;

	std::vector<ExecutionInfo::Barrier> barriers;
	std::vector<std::size_t> barrierOffsets;

	for (auto& taskDependency : m_dependencies.at(outputImage)) {
		taskStack.push(taskDependency.taskIndex);
	}

	while (!taskStack.empty()) {
		uint32_t taskIndex = taskStack.top();
		taskStack.pop();

		if (visitedTasks.contains(taskIndex)) continue;

		auto [taskBarriers, referencedTasks] = getBarriers(taskIndex);

		auto notVisitedTasks =
			referencedTasks |
			std::views::filter([&visitedTasks](uint32_t task) {
				return !visitedTasks.contains(task);
			});

		if (!notVisitedTasks.empty()) {
			taskStack.push(taskIndex);

			for (uint32_t index : notVisitedTasks) {
				taskStack.push(index);
			}
			continue;
		}

		barrierOffsets.push_back(barriers.size());
		barrierOffsets.push_back(barriers.size() + taskBarriers.size());
		barriers.insert(
			barriers.end(), taskBarriers.begin(), taskBarriers.end()
		);

		tasks.push_back(taskIndex);
		visitedTasks.insert(taskIndex);

		auto& taskData = m_data.taskData[taskIndex];
		for (int i = taskData.inputs.offset;
		     i < taskData.inputs.offset + taskData.inputs.count;
		     i++) {
			ResourceIndex resourceIndex = m_data.taskDependencies[i].first;
			if (m_data.images.contains(resourceIndex))
				referencedImages.insert(resourceIndex);
		}
		for (int i = taskData.outputs.offset;
		     i < taskData.outputs.offset + taskData.outputs.count;
		     i++) {
			ResourceIndex resourceIndex = m_data.taskDependencies[i].first;
			if (m_data.images.contains(resourceIndex))
				referencedImages.insert(resourceIndex);
		}
	}

	std::unordered_map<ResourceIndex, vk::ImageMemoryBarrier2> imageBarriers;
	std::unordered_map<ResourceIndex, ResourceUsage::Type> finalUsages;
	for (ResourceIndex image : referencedImages) {
		// Start from the last image reference, we go backward until we have an
		// active task, if the layout is different from the last build we
		// syncronize with a new initialization barrier
		for (int i = m_dependencies.at(image).size() - 1; i >= 0; i--) {
			const TaskResourceDependency& dep = m_dependencies.at(image)[i];

			if (!visitedTasks.contains(dep.taskIndex)) continue;

			ResourceUsage::Type pastLastUsage = m_data.finalUsages.at(image);

			finalUsages[image] = dep.usage;
			if (dep.usage == pastLastUsage) break;

			imageBarriers[image] = {
				.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
				.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
				.oldLayout = ResourceUsage::GetAccess(pastLastUsage).layout,
				.newLayout = ResourceUsage::GetAccess(dep.usage).layout,
			};
			break;
		}
	}

	return {
		.tasks = tasks,
		.barriers = barriers,
		.barrierOffsets = barrierOffsets,
		.finalUsages = finalUsages,
	};
}
