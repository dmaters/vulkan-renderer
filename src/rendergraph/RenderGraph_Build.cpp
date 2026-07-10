#include <algorithm>
#include <cassert>
#include <ranges>
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

struct ExplorationData {
	ExecutionInfo::Resources resources;
	ExecutionInfo::References references;
	std::vector<TaskIndex> tasks;
};

class GraphExplorer : Task::SetupContext::ResourceProvider::TaskManager {
private:
	const std::unordered_map<TaskIndex, Task>& m_tasks;
	std::unordered_map<TaskIndex, std::vector<std::byte>>& m_taskData;
	const Scene& m_scene;
	Task::SetupContext::ResourceProvider m_resourceProvider;

	std::vector<TaskIndex> m_taskStack;
	std::vector<bool> m_exploredTasks;
	ExplorationData m_data;

public:
	GraphExplorer(
		const std::unordered_map<TaskIndex, Task>& tasks,
		std::unordered_map<TaskIndex, std::vector<std::byte>>& taskData,
		const std::vector<TaskIndex>& optionalTasks,
		const Scene& scene,
		rendergraph::internal::ResourceIndexer indexer
	) :
		m_tasks(tasks), m_taskData(taskData), m_scene(scene), m_resourceProvider(indexer, *this) {
		m_exploredTasks = std::vector<bool>(tasks.size());
		for (auto optionalTask : optionalTasks) {
			explore(optionalTask);
		}
	}

	void explore(TaskIndex task) {
		m_exploredTasks[task] = true;

		Task::DataProvider dataProvider(m_taskData);

		Task::SetupContext context {
			.task = task,
			.scene = m_scene,
			.resourceProvider = m_resourceProvider,
			.dataProvider = dataProvider,
		};

		Task::Dependencies deps = m_tasks.at(task).setup(context);

		m_data.references.inputs[task] = deps.inputs;
		m_data.references.outputs[task] = deps.outputs;

		m_taskStack.push_back(task);
	}

	rendergraph::ResourceIndex getResource(TaskIndex index, std::size_t slot) override {
		if (!m_exploredTasks[index]) explore(index);
		return m_data.references.outputs[index][slot].resource;
	}

	ExplorationData getData() && {
		m_data.tasks = std::move(m_taskStack);
		m_data.resources = std::move(m_resourceProvider).getCompiledResources();

		return m_data;
	}
};

struct TaskReference {
	TaskIndex task;
	bool isTaskPreviousSubmission;
};

ExecutionInfo RenderGraph::build(
	TaskIndex outputTask, const std::vector<TaskIndex>& optionalTasks, const Scene& scene
) {
	GraphExplorer explorer(m_data.tasks, m_data.taskData, optionalTasks, scene, m_data.indexer);

	explorer.explore(outputTask);

	auto [resources, references, tasks] = std::move(explorer).getData();

	std::unordered_map<rendergraph::ResourceIndex, ResourceUsage::Type> resourceUsages;

	for (TaskIndex task : tasks) {
		for (auto [index, usage] : references.inputs[task]) {
			resourceUsages[index] = usage;
		}
		for (auto [index, usage] : references.outputs[task]) {
			resourceUsages[index] = usage;
		}
	}

	std::unordered_map<TaskIndex, std::vector<ExecutionInfo::Barrier>> barriers;
	std::vector<ExecutionInfo::Barrier> taskBarriers;

	for (TaskIndex task : tasks) {
		taskBarriers.clear();
		for (auto [index, usage] : references.inputs[task]) {
			taskBarriers.push_back(
				{
					.previousUsage = resourceUsages[index],
					.currentUsage = usage,
					.index = index,
				}
			);

			resourceUsages[index] = usage;
		}
		for (auto [index, usage] : references.outputs[task]) {
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
	for (auto [image, _] : resources.images) {
		initializationBarriers.push_back(
			{
				.previousUsage = ResourceUsage::Type::Undefined,
				.currentUsage = resourceUsages[image],
				.index = image,
			}
		);
	}

	return {
		.resources = resources,
		.references = references,
		.outputImage = references.outputs[outputTask][0].resource,
		.tasks = tasks,
		.barriers = barriers,
		.initializationBarriers = initializationBarriers,
	};
}
