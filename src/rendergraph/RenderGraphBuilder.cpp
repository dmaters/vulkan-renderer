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
#include "tasks/MemoryBarrier.hpp"
#include "tasks/Task.hpp"
#include "tasks/TaskContext.hpp"

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

	auto [currentStage, currentAccess, currentLayout] =
		ResourceUsage::GetAccess(currentUsage);

	for (int i = 1; i <= taskCount; i++) {
		int previousUsageIndex =
			(taskCount + (currentUsageIndex - i)) % taskCount;

		ResourceUsage::Type previousUsage =
			resourceDependencies[previousUsageIndex].usage;

		if ((currentUsage == previousUsage) &&
		    ResourceUsage::IsReadAccess(currentUsage))
			continue;

		auto [previousStage, previousAccess, previousLayout] =
			ResourceUsage::GetAccess(
				resourceDependencies[previousUsageIndex].usage
			);

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

RenderGraphBuilder::Barrier RenderGraphBuilder::getBarrier(
	uint32_t taskIndex
) const {
	std::unordered_map<ResourceIndex, vk::ImageMemoryBarrier2> imageBarriers;
	std::unordered_map<ResourceIndex, vk::BufferMemoryBarrier2> bufferBarriers;

	std::unordered_set<uint32_t> previousTasks;

	auto [inputOffset, inputCount] = m_data.taskData[taskIndex].inputs;
	for (int i = inputOffset; i < inputOffset + inputCount; i++) {
		auto [index, dependency] = m_data.taskDependencies[i];

		auto access = getResourceBarrier(taskIndex, index);

		if (!access.barrier.has_value()) continue;

		if (!access.isTaskPreviousSubmission && access.task != taskIndex)
			previousTasks.insert(access.task);

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

		auto access = getResourceBarrier(taskIndex, index);

		if (!access.barrier.has_value()) continue;

		if (!access.isTaskPreviousSubmission && access.task != taskIndex)
			previousTasks.insert(access.task);

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
	std::stack<std::string> debugStack;
	std::unordered_set<uint32_t> visitedTasks;

	std::unordered_set<ResourceIndex> referencedImages;

	std::vector<TaskIndex> tasks;
	std::vector<std::string> tasksDebug;

	std::vector<GraphData::TaskData> barriersData;
	std::vector<Task> barrierTasks;

	uint32_t baseTaskOffset = m_data.tasks.size();

	for (auto& taskDependency : m_dependencies.at(outputImage)) {
		taskStack.push(taskDependency.taskIndex);
		debugStack.push(m_data.taskData[taskDependency.taskIndex].name);
	}

	while (!taskStack.empty()) {
		uint32_t taskIndex = taskStack.top();
		taskStack.pop();
		debugStack.pop();

		if (visitedTasks.contains(taskIndex)) continue;

		auto [barrierData, barrierTask, referencedTasks] =
			getBarrier(taskIndex);

		auto notVisitedTasks =
			referencedTasks |
			std::views::filter([&visitedTasks](uint32_t task) {
				return !visitedTasks.contains(task);
			});

		if (!notVisitedTasks.empty()) {
			taskStack.push(taskIndex);
			debugStack.push(m_data.taskData[taskIndex].name);

			for (uint32_t index : notVisitedTasks) {
				taskStack.push(index);
				debugStack.push(m_data.taskData[index].name);
			}
			continue;
		}

		tasks.push_back(taskIndex);
		tasksDebug.push_back(m_data.taskData[taskIndex].name);
		visitedTasks.insert(taskIndex);

		if (barrierData.has_value()) {
			std::string barrier;
			for (auto& task : referencedTasks) {
				barrier += "|" + m_data.taskData[task].name;
			};
			tasksDebug.push_back(
				m_data.taskData[taskIndex].name + " " + barrier
			);
			tasks.push_back(baseTaskOffset + barrierTasks.size());
			barrierTasks.push_back(std::move(barrierTask.value()));
			barriersData.push_back(barrierData.value());
		}

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
		.data = barriersData,
		.barriers = barrierTasks,
		.initializationTask = [imageBarriers = std::move(imageBarriers)](
								  TaskContext& context
							  ) { MemoryBarrier(context, {}, imageBarriers); },
		.finalUsages = finalUsages,
	};
}
