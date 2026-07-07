#include <cassert>
#include <stack>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "DataProvider.hpp"
#include "GraphData.hpp"
#include "RenderGraph.hpp"
#include "ResourceIndex.hpp"
#include "ResourceUsage.hpp"
#include "SetupContext.hpp"
#include "Task.hpp"
#include "scene/Scene.hpp"

using namespace rendergraph::internal;

struct InstanceData {
	ExecutionInfo::Resources resources;
	ExecutionInfo::References references;
};

InstanceData buildTasks(
	const Scene& scene,
	const std::vector<TaskIndex>& unorderedTasks,
	const ResourceIndexer& indexer,
	const std::unordered_map<TaskIndex, Task>& tasks,
	std::unordered_map<TaskIndex, std::vector<std::byte>>& taskData
) {
	ExecutionInfo::References references;
	Task::SetupContext::ResourceProvider resourceProvider(indexer, references);
	Task::DataProvider dataProvider(taskData);

	for (TaskIndex task : unorderedTasks) {
		Task::SetupContext context {
			.task = task,
			.scene = scene,
			.resourceProvider = resourceProvider,
			.dataProvider = dataProvider,
		};

		Task::Dependencies deps = tasks.at(task).setup(context);

		references.inputs[task] = deps.inputs;
		references.outputs[task] = deps.outputs;
	}
	ExecutionInfo::Resources resources =
		std::move(resourceProvider).getCompiledResources();

	return {
		.resources = resources,
		.references = references,
	};
}

struct TaskReference {
	TaskIndex task;
	bool isTaskPreviousSubmission;
};

struct DependencyGraph {
	struct Reference {
		TaskIndex task;
		ResourceUsage::Type usage;
	};
	std::unordered_map<rendergraph::ResourceIndex, std::vector<Reference>>
		taskReferencesByResources;

	struct ReferenceCount {
		uint8_t read = 0;
		uint8_t write = 0;
	};

	std::unordered_map<rendergraph::ResourceIndex, ReferenceCount>
		referenceCounts;
};

TaskReference getPreviousTask(
	const DependencyGraph& graph,
	uint32_t taskIndex,
	rendergraph::ResourceIndex resourceIndex
) {
	int taskUsageIndex = -1;
	auto references = graph.taskReferencesByResources.at(resourceIndex);
	for (int i = 0; i < references.size(); i++) {
		if (references[i].task == taskIndex) {
			taskUsageIndex = i;
			break;
		}
	}
	assert(taskUsageIndex != -1);
	int previousUsageIndex = taskUsageIndex;

	for (int i = 1; i <= references.size(); i++) {
		int current =
			(taskUsageIndex + references.size() - i) % references.size();
		if (current < graph.referenceCounts.at(resourceIndex).write) {
			previousUsageIndex = current;
			break;
		}
	}

	return {
		.task = references[previousUsageIndex].task,
		.isTaskPreviousSubmission = previousUsageIndex >= taskUsageIndex,
	};
}

std::vector<TaskIndex> getConnectedTasks(
	TaskIndex taskIndex,
	ExecutionInfo::References& references,
	const DependencyGraph& graph
) {
	std::vector<TaskIndex> previousTasks;

	for (auto [index, dependency] : references.inputs[taskIndex]) {
		auto access = getPreviousTask(graph, taskIndex, index);

		if (access.task == taskIndex || access.isTaskPreviousSubmission)
			continue;

		previousTasks.push_back(access.task);
	}
	for (auto [index, dependency] : references.outputs[taskIndex]) {
		auto access = getPreviousTask(graph, taskIndex, index);

		if (access.task == taskIndex || access.isTaskPreviousSubmission)
			continue;

		previousTasks.push_back(access.task);
	}
	return previousTasks;
}

DependencyGraph buildDependencyGraph(
	const std::vector<TaskIndex>& unorderedTasks,
	const ExecutionInfo::References& references
) {
	struct TempReference {
		enum class ReferenceType {
			Read,
			Write,
		};
		ReferenceType type;
		ResourceUsage::Type usage;
		TaskIndex task;

		rendergraph::ResourceIndex resource;
	};
	std::vector<TempReference> tempReferences;

	std::unordered_map<
		rendergraph::ResourceIndex,
		DependencyGraph::ReferenceCount>
		usageCount;

	// We store all the references, with types, and we count the reference
	// type so to build the map later
	for (TaskIndex task : unorderedTasks) {
		for (auto [index, usage] : references.inputs.at(task)) {
			tempReferences.push_back(
				{
					.type = TempReference::ReferenceType::Read,
					.usage = usage,
					.task = task,
					.resource = index,
				}
			);

			usageCount[index].read += 1;
		}

		for (auto [index, usage] : references.outputs.at(task)) {
			tempReferences.push_back(
				{
					.type = TempReference::ReferenceType::Write,
					.usage = usage,
					.task = task,
					.resource = index,
				}
			);

			usageCount[index].write += 1;
		}
	}

	std::sort(
		tempReferences.begin(), tempReferences.end(), [](auto& x, auto& y) {
			bool isOrderFlipped =
				x.type == TempReference::ReferenceType::Read &&
				y.type == TempReference::ReferenceType::Write;
			return x.resource < y.resource ||
		           (x.resource == y.resource && !(isOrderFlipped) &&
		            x.task < y.task);
		}
	);

	std::unordered_map<
		rendergraph::ResourceIndex,
		std::vector<DependencyGraph::Reference>>
		resourceTaskReferences;

	std::vector<DependencyGraph::Reference> perResourcesReferences;

	for (int i = 0; i < tempReferences.size(); i++) {
		rendergraph::ResourceIndex resource = tempReferences[i].resource;

		resourceTaskReferences[resource].push_back(
			{
				.task = tempReferences[i].task,
				.usage = tempReferences[i].usage,
			}
		);
	}

	return DependencyGraph {
		.taskReferencesByResources = resourceTaskReferences,
		.referenceCounts = usageCount,
	};
}

// BUG: the construction algorithm sorts the references by read-write
// for a proper execution order but if two tasks are connected by read-write
// and write-write the sorting will apply only for the first reference
// This creates the case where the tasks with the order B - A
// (read-write) A -> B,
// (write-write)B -> A
// that hangs during graph flattening, so to avoid hangups it's mandatory to
// keep some sort of order in the tasks
// The renderer is small in scope, so we can offset that to definition time
// (ensure that tasks are somewhat ordered already)
ExecutionInfo RenderGraph::build(
	const std::vector<TaskIndex>& taskList, const Scene& scene
) {
	assert(taskList.size() > 0);

	InstanceData instanceData = buildTasks(
		scene, taskList, m_data.indexer, m_data.tasks, m_data.taskData
	);

	DependencyGraph graph =
		buildDependencyGraph(taskList, instanceData.references);

	std::vector<TaskIndex> tasks;

	std::unordered_map<rendergraph::ResourceIndex, ResourceUsage::Type>
		resourceUsages;
	std::stack<TaskIndex, std::vector<TaskIndex>> taskStack;
	std::vector<bool> visitedTasks(m_data.tasks.size());

	rendergraph::ResourceIndex outputImage =
		instanceData.references.outputs[taskList.back()].front().resource;

	for (auto [task, usage] : graph.taskReferencesByResources[outputImage]) {
		taskStack.push(task);
	}

	while (!taskStack.empty()) {
		TaskIndex taskIndex = taskStack.top();
		taskStack.pop();

		if (visitedTasks[taskIndex]) continue;

		std::vector<TaskIndex> referencedTasks =
			getConnectedTasks(taskIndex, instanceData.references, graph);

		bool finalTask = true;
		for (TaskIndex referencedTask : referencedTasks) {
			if (visitedTasks[referencedTask]) continue;
			if (finalTask) {
				finalTask = false;
				taskStack.push(taskIndex);
			}

			taskStack.push(referencedTask);
		}

		if (!finalTask) continue;

		tasks.push_back(taskIndex);
		visitedTasks[taskIndex] = true;

		for (auto [index, usage] : instanceData.references.inputs[taskIndex]) {
			resourceUsages[index] = usage;
		}
		for (auto [index, usage] : instanceData.references.outputs[taskIndex]) {
			resourceUsages[index] = usage;
		}
	}

	std::unordered_map<TaskIndex, std::vector<ExecutionInfo::Barrier>> barriers;
	std::vector<ExecutionInfo::Barrier> taskBarriers;

	for (TaskIndex task : tasks) {
		taskBarriers.clear();

		for (auto [index, usage] : instanceData.references.inputs[task]) {
			taskBarriers.push_back(
				{
					.previousUsage = resourceUsages[index],
					.currentUsage = usage,
					.index = index,
				}
			);

			resourceUsages[index] = usage;
		}
		for (auto [index, usage] : instanceData.references.outputs[task]) {
			taskBarriers.push_back(
				{
					.previousUsage = resourceUsages[index],
					.currentUsage = usage,
					.index = index,
				}
			);

			resourceUsages[index] = usage;
		}
		barriers[task] = taskBarriers;
	}

	std::vector<ExecutionInfo::Barrier> initializationBarriers;
	for (auto [image, _] : instanceData.resources.images) {
		initializationBarriers.push_back(
			{
				.previousUsage = ResourceUsage::Type::Undefined,
				.currentUsage = resourceUsages[image],
				.index = image,
			}
		);
	}

	return {
		.resources = instanceData.resources,
		.references = instanceData.references,
		.outputImage = outputImage,
		.tasks = tasks,
		.barriers = barriers,
		.initializationBarriers = initializationBarriers,
	};
}
