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

struct RenderGraphBuilder::TaskReference {
	uint32_t task;
	bool isTaskPreviousSubmission;
};

RenderGraphBuilder::TaskReference RenderGraphBuilder::getPreviousTask(
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

		return TaskReference {
			.task = resourceDependencies[previousUsageIndex].taskIndex,
			.isTaskPreviousSubmission = previousUsageIndex >= currentUsageIndex,
		};
	}
	return {
		.task = resourceDependencies[currentUsageIndex].taskIndex,
		.isTaskPreviousSubmission = true,
	};
}

std::unordered_set<uint32_t> RenderGraphBuilder::getReferencedTasks(
	uint32_t taskIndex
) const {
	std::vector<ExecutionInfo::Barrier> barriers;
	std::unordered_set<uint32_t> previousTasks;

	auto [inputOffset, inputCount] = m_data.taskData[taskIndex].inputs;
	for (int i = inputOffset; i < inputOffset + inputCount; i++) {
		auto [index, dependency] = m_data.taskDependencies[i];

		auto access = getPreviousTask(taskIndex, index);

		if (access.task == taskIndex || access.isTaskPreviousSubmission)
			continue;

		previousTasks.insert(access.task);
	}
	auto [outputOffset, outputCount] = m_data.taskData[taskIndex].outputs;
	for (int i = outputOffset; i < outputOffset + outputCount; i++) {
		auto [index, dependency] = m_data.taskDependencies[i];

		auto access = getPreviousTask(taskIndex, index);

		if (access.task == taskIndex || access.isTaskPreviousSubmission)
			continue;

		previousTasks.insert(access.task);
	}
	return previousTasks;
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
	std::vector<TaskIndex> tasks;

	std::unordered_map<ResourceIndex, ResourceUsage::Type> resourceUsages;
	std::unordered_set<ResourceIndex> referencedImages;

	std::stack<uint32_t> taskStack;
	std::unordered_set<uint32_t> visitedTasks;

	for (auto& taskDependency : m_dependencies.at(outputImage)) {
		taskStack.push(taskDependency.taskIndex);
	}

	while (!taskStack.empty()) {
		uint32_t taskIndex = taskStack.top();
		taskStack.pop();

		if (visitedTasks.contains(taskIndex)) continue;

		std::unordered_set<uint32_t> referencedTasks =
			getReferencedTasks(taskIndex);

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

		tasks.push_back(taskIndex);
		visitedTasks.insert(taskIndex);

		auto& taskData = m_data.taskData[taskIndex];
		for (int i = taskData.inputs.offset;
		     i < taskData.inputs.offset + taskData.inputs.count;
		     i++) {
			auto [resourceIndex, usage] = m_data.taskDependencies[i];
			if (m_data.images.contains(resourceIndex))
				referencedImages.insert(resourceIndex);

			resourceUsages[resourceIndex] = usage;
		}
		for (int i = taskData.outputs.offset;
		     i < taskData.outputs.offset + taskData.outputs.count;
		     i++) {
			auto [resourceIndex, usage] = m_data.taskDependencies[i];

			if (m_data.images.contains(resourceIndex))
				referencedImages.insert(resourceIndex);

			resourceUsages[resourceIndex] = usage;
		}
	}

	std::vector<ExecutionInfo::Barrier> barriers;
	std::vector<std::size_t> barrierOffsets;

	for (TaskIndex task : tasks) {
		std::vector<ExecutionInfo::Barrier> taskBarriers;

		auto& taskData = m_data.taskData[task];

		barrierOffsets.push_back(barriers.size());

		for (int i = taskData.inputs.offset;
		     i < taskData.inputs.offset + taskData.inputs.count;
		     i++) {
			auto [resourceIndex, usage] = m_data.taskDependencies[i];

			barriers.push_back(
				{
					.previousUsage = resourceUsages[resourceIndex],
					.currentUsage = usage,
					.index = resourceIndex,
				}
			);

			resourceUsages[resourceIndex] = usage;
		}
		for (int i = taskData.outputs.offset;
		     i < taskData.outputs.offset + taskData.outputs.count;
		     i++) {
			auto [resourceIndex, usage] = m_data.taskDependencies[i];

			barriers.push_back(
				{
					.previousUsage = resourceUsages[resourceIndex],
					.currentUsage = usage,
					.index = resourceIndex,
				}
			);

			resourceUsages[resourceIndex] = usage;
		}

		barrierOffsets.push_back(barriers.size());
	}

	std::unordered_map<ResourceIndex, ResourceUsage::Type> finalUsages;
	std::vector<ExecutionInfo::Barrier> initializationBarriers;
	for (ResourceIndex image : referencedImages) {
		finalUsages[image] = resourceUsages[image];

		if (resourceUsages[image] == m_data.finalUsages.at(image)) continue;

		initializationBarriers.push_back(
			{
				.previousUsage = m_data.finalUsages.at(image),
				.currentUsage = resourceUsages[image],
				.index = image,
			}
		);
	}

	return {
		.tasks = tasks,
		.barriers = barriers,
		.barrierOffsets = barrierOffsets,
		.initializationBarriers = initializationBarriers,
		.finalUsages = finalUsages,
	};
}
