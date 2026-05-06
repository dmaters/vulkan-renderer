#include "RenderGraphBuilder.hpp"

#include <cassert>
#include <stack>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "GraphData.hpp"
#include "ResourceUsage.hpp"
#include "tasks/Task.hpp"

using namespace rendergraph::internal;

struct TaskReference {
	TaskIndex task;
	bool isTaskPreviousSubmission;
};

struct DependencyGraph {
	// We store all the resource references contiguosly, read references are
	// stored after the write references

	// m_references contains an additional dummy value , so to easily get
	// the references count per resource you can
	// [n + 1].baseOffset - [n].baseOffset
	// with no bound checking

	struct Reference {
		TaskIndex task;
		ResourceUsage::Type usage;
	};
	std::vector<Reference> references;
	struct Offsets {
		uint16_t baseOffset = 0;
		uint8_t readOffset = 0;
	};

	// The following two vectors are coupled by position
	// m_resources is sorted, so binary search can be used to get the position
	std::vector<Offsets> offsets;
	std::vector<ResourceIndex> resources;

	std::span<Reference> getReferences(ResourceIndex index) {
		auto itr = std::lower_bound(resources.begin(), resources.end(), index);
		int pos = std::distance(resources.begin(), itr);

		return std::span<Reference>(
			references.data() + offsets[pos].baseOffset,
			references.data() + offsets[pos + 1].baseOffset
		);
	}
};

inline std::size_t getResourceIndexPosition(
	const std::vector<ResourceIndex>& indices, ResourceIndex index
) {
	auto itr = std::lower_bound(indices.begin(), indices.end(), index);
	return std::distance(indices.begin(), itr);
}

TaskReference getPreviousTask(
	const DependencyGraph& graph,
	uint32_t taskIndex,
	ResourceIndex resourceIndex
) {
	std::size_t pos = getResourceIndexPosition(graph.resources, resourceIndex);
	int currentUsageIndex = -1;

	for (int i = graph.offsets[pos].baseOffset;
	     i < graph.offsets[pos + 1].baseOffset;
	     i++) {
		if (graph.references[i].task == taskIndex) {
			currentUsageIndex = i;
			break;
		}
	}
	assert(currentUsageIndex != -1);

	int taskCount =
		graph.offsets[pos + 1].baseOffset - graph.offsets[pos].baseOffset;

	int localOffset = currentUsageIndex - graph.offsets[pos].baseOffset;

	int previousUsageIndex = currentUsageIndex;
	for (int i = 1; i <= taskCount; i++) {
		int previousLocalOffset = (localOffset + taskCount - i) % taskCount;
		if (previousLocalOffset < graph.offsets[pos].readOffset) {
			previousUsageIndex =
				previousLocalOffset + graph.offsets[pos].baseOffset;
			break;
		}
	}

	return {
		.task = graph.references[previousUsageIndex].task,
		.isTaskPreviousSubmission = previousUsageIndex >= currentUsageIndex,
	};
}

std::vector<TaskIndex> getConnectedTasks(
	TaskIndex taskIndex, const GraphData& data, const DependencyGraph& graph
) {
	std::vector<TaskIndex> previousTasks;

	auto [inputOffset, inputCount] = data.taskData[taskIndex].inputs;
	for (int i = inputOffset; i < inputOffset + inputCount; i++) {
		auto [index, dependency] = data.taskResources[i];

		auto access = getPreviousTask(graph, taskIndex, index);

		if (access.task == taskIndex || access.isTaskPreviousSubmission)
			continue;

		previousTasks.push_back(access.task);
	}
	auto [outputOffset, outputCount] = data.taskData[taskIndex].outputs;
	for (int i = outputOffset; i < outputOffset + outputCount; i++) {
		auto [index, dependency] = data.taskResources[i];

		auto access = getPreviousTask(graph, taskIndex, index);

		if (access.task == taskIndex || access.isTaskPreviousSubmission)
			continue;

		previousTasks.push_back(access.task);
	}
	return previousTasks;
}

DependencyGraph buildDependencyGraph(
	const std::vector<TaskIndex>& unorderedTasks, const GraphData& data
) {
	struct TempReference {
		enum class ReferenceType {
			Read,
			Write,
		};
		ReferenceType type;
		ResourceUsage::Type usage;
		TaskIndex task;
		ResourceIndex resource;
	};
	std::vector<TempReference> tempReferences;

	struct ReferenceCount {
		uint8_t read = 0;
		uint8_t write = 0;
	};
	std::vector<ReferenceCount> resourceUsageCount(data.resourceCount);
	std::vector<ResourceIndex> resources;

	// We store all the references, with types, and we count the reference type
	// so to build the offset map later
	for (TaskIndex task : unorderedTasks) {
		const GraphData::TaskData& taskData = data.taskData[task];

		for (int i = taskData.inputs.offset;
		     i < taskData.inputs.offset + taskData.inputs.count;
		     i++) {
			auto [index, usage] = data.taskResources[i];

			tempReferences.push_back(
				{
					.type = TempReference::ReferenceType::Read,
					.usage = usage,
					.task = task,
					.resource = index,
				}
			);
			if (resourceUsageCount[index].read == 0 &&
			    resourceUsageCount[index].write == 0)
				resources.push_back(index);
			resourceUsageCount[index].read += 1;
		}

		for (int i = taskData.outputs.offset;
		     i < taskData.outputs.offset + taskData.outputs.count;
		     i++) {
			auto [index, usage] = data.taskResources[i];

			tempReferences.push_back(
				{
					.type = TempReference::ReferenceType::Write,
					.usage = usage,
					.task = task,
					.resource = index,
				}
			);
			if (resourceUsageCount[index].read == 0 &&
			    resourceUsageCount[index].write == 0)
				resources.push_back(index);
			resourceUsageCount[index].write += 1;
		}
	}

	// By sorting the resources we can easily search the position of one by
	// binary search
	std::sort(resources.begin(), resources.end());

	std::vector<DependencyGraph::Reference> references(tempReferences.size());
	std::vector<DependencyGraph::Offsets> offsets(resources.size());

	uint16_t baseOffset = 0;
	for (int i = 0; i < resources.size(); i++) {
		ResourceIndex index = resources[i];
		offsets[i] = {
			.baseOffset = baseOffset,
			.readOffset = resourceUsageCount[index].write,
		};

		baseOffset +=
			resourceUsageCount[index].read + resourceUsageCount[index].write;

		resourceUsageCount[index] = {};
	}

	// Dummy element so we can always have a top bound (n+1) for loops
	offsets.push_back(
		{ .baseOffset = static_cast<uint16_t>(references.size()) }
	);

	for (TempReference reference : tempReferences) {
		std::size_t pos =
			getResourceIndexPosition(resources, reference.resource);
		DependencyGraph::Offsets resourceOffsets = offsets[pos];

		std::size_t offset = 0;
		switch (reference.type) {
			case TempReference::ReferenceType::Write:
				offset = resourceOffsets.baseOffset +
				         resourceUsageCount[reference.resource].write;

				resourceUsageCount[reference.resource].write += 1;
				break;

			case TempReference::ReferenceType::Read:
				offset = resourceOffsets.baseOffset +
				         resourceOffsets.readOffset +
				         resourceUsageCount[reference.resource].read;
				resourceUsageCount[reference.resource].read += 1;

				break;
		}
		references[offset] = {
			.task = reference.task,
			.usage = reference.usage,
		};
	}

	return DependencyGraph {
		.references = references,
		.offsets = offsets,
		.resources = resources,
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
ExecutionInfo RenderGraphBuilder::getTasks(
	const std::vector<TaskIndex> taskList, ResourceIndex outputImage
) const {
	DependencyGraph graph = buildDependencyGraph(taskList, m_data);

	assert(
		std::binary_search(
			graph.resources.begin(), graph.resources.end(), outputImage
		)
	);

	std::vector<TaskIndex> tasks;

	std::vector<ResourceUsage::Type> resourceUsages(m_data.resourceCount);
	std::stack<TaskIndex, std::vector<TaskIndex>> taskStack;
	std::vector<bool> visitedTasks(m_data.tasks.size());

	for (auto [task, usage] : graph.getReferences(outputImage)) {
		taskStack.push(task);
	}

	while (!taskStack.empty()) {
		TaskIndex taskIndex = taskStack.top();
		taskStack.pop();

		if (visitedTasks[taskIndex]) continue;

		std::vector<TaskIndex> referencedTasks =
			getConnectedTasks(taskIndex, m_data, graph);

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

		auto& taskData = m_data.taskData[taskIndex];
		for (int i = taskData.inputs.offset;
		     i < taskData.inputs.offset + taskData.inputs.count;
		     i++) {
			auto [resourceIndex, usage] = m_data.taskResources[i];

			resourceUsages[resourceIndex] = usage;
		}
		for (int i = taskData.outputs.offset;
		     i < taskData.outputs.offset + taskData.outputs.count;
		     i++) {
			auto [resourceIndex, usage] = m_data.taskResources[i];

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
			auto [resourceIndex, usage] = m_data.taskResources[i];

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
			auto [resourceIndex, usage] = m_data.taskResources[i];

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
	for (ResourceIndex resource : graph.resources) {
		if (!m_data.images.contains(resource)) continue;

		ResourceIndex image = resource;

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
