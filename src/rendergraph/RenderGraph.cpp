#include "RenderGraph.hpp"

#include <cassert>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "DataProvider.hpp"
#include "GraphData.hpp"
#include "RenderGraph.hpp"
#include "RenderingConfiguration.hpp"
#include "ResourceIndex.hpp"
#include "ResourceUsage.hpp"
#include "SetupContext.hpp"
#include "Swapchain.hpp"
#include "Task.hpp"
#include "rendergraph/GraphData.hpp"
#include "rendergraph/Task.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

RenderGraph::RenderGraph(
	const RenderingConfiguration& renderingConfiguration,
	TaskRegister& taskRegister,
	Swapchain& swapchain,
	ResourceManager& resourceManager,
	MaterialManager& materialManager
) :
	m_swapchain(swapchain),
	m_resourceManager(resourceManager),
	m_materialManager(materialManager),
	m_renderingConfiguration(renderingConfiguration),
	m_taskRegister(taskRegister) {}

void RenderGraph::setImage(rendergraph::ResourceIndex index, ImageHandle handle) {
	m_externalImages[index] = handle;
	m_resourceManager.setName(*m_taskRegister.getStaticResources().names[index], handle);
}
void RenderGraph::setBuffer(rendergraph::ResourceIndex index, BufferHandle handle) {
	m_externalBuffers[index] = handle;
	m_resourceManager.setName(*m_taskRegister.getStaticResources().names[index], handle);
}

struct ExplorationData {
	rendergraph::internal::ExecutionInfo::Resources resources;
	rendergraph::internal::ExecutionInfo::References references;
	std::vector<TaskIndex> tasks;
};

class GraphExplorer : Task::SetupContext::ResourceProvider::TaskManager {
private:
	const RenderingConfiguration& m_renderingConfiguration;
	const Scene& m_scene;
	TaskRegister& m_taskRegister;
	Task::SetupContext::ResourceProvider m_resourceProvider;

	std::vector<TaskIndex> m_taskStack;
	std::vector<bool> m_exploredTasks;
	ExplorationData m_data;

public:
	GraphExplorer(
		const RenderingConfiguration& renderingConfiguration,
		TaskRegister& taskRegister,
		const std::vector<TaskIndex>& optionalTasks,
		const Scene& scene
	) :
		m_renderingConfiguration(renderingConfiguration),
		m_scene(scene),
		m_taskRegister(taskRegister),
		m_resourceProvider(*this, taskRegister) {
		m_exploredTasks = std::vector<bool>(taskRegister.getTasks().tasks.size());
		for (auto optionalTask : optionalTasks) {
			explore(optionalTask);
		}
	}

	void explore(TaskIndex task) {
		if (m_exploredTasks[task]) return;
		m_exploredTasks[task] = true;

		Task::DataProvider dataProvider(m_taskRegister);

		Task::SetupContext context {
			.task = task,
			.scene = m_scene,
			.resourceProvider = m_resourceProvider,
			.dataProvider = dataProvider,
			.renderingConfiguration = m_renderingConfiguration,
		};

		Task::Dependencies deps = m_taskRegister.getTasks().tasks[task].setup(context);

		m_data.references.inputs[task] = deps.inputs;
		m_data.references.outputs[task] = deps.outputs;

		m_taskStack.push_back(task);
	}

	rendergraph::ResourceIndex getResource(TaskIndex index, std::size_t slot) override {
		explore(index);
		return m_data.references.outputs[index][slot].resource;
	}

	ExplorationData getData() && {
		m_data.tasks = std::move(m_taskStack);
		m_data.resources = std::move(m_resourceProvider).getCompiledResources();

		return m_data;
	}
};

rendergraph::internal::ExecutionInfo RenderGraph::build(
	TaskIndex outputTask, const std::vector<TaskIndex>& optionalTasks, const Scene& scene
) {
	GraphExplorer explorer(m_renderingConfiguration, m_taskRegister, optionalTasks, scene);

	explorer.explore(outputTask);

	auto [resources, references, tasks] = std::move(explorer).getData();

	IndexMap<rendergraph::ResourceIndex, ResourceUsage::Type> resourceUsages;

	for (TaskIndex task : tasks) {
		for (auto [index, usage] : references.inputs[task]) {
			resourceUsages[index] = usage;
		}
		for (auto [index, usage] : references.outputs[task]) {
			resourceUsages[index] = usage;
		}
	}

	IndexMap<TaskIndex, std::vector<rendergraph::internal::ExecutionInfo::Barrier>> barriers;
	std::vector<rendergraph::internal::ExecutionInfo::Barrier> taskBarriers;

	for (TaskIndex task : tasks) {
		taskBarriers.clear();
		for (auto [index, usage] : references.inputs[task]) {
			if (usage == ResourceUsage::Type::None) continue;

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
			if (usage == ResourceUsage::Type::None) continue;
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

	std::vector<rendergraph::internal::ExecutionInfo::Barrier> initializationBarriers;
	for (auto [image, _] : resources.images.items()) {
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

void RenderGraph::update(TaskIndex outputTask, const std::vector<TaskIndex>& optionalTasks, const Scene& scene) {
	rendergraph::internal::ExecutionInfo info = build(outputTask, optionalTasks, scene);
	info.resources.externalImages = m_externalImages;
	info.resources.externalBuffers = m_externalBuffers;

	m_runner.emplace(m_swapchain, m_resourceManager, m_materialManager, m_renderingConfiguration, info, m_taskRegister);
}

bool RenderGraph::submit(const Scene& scene) { return m_runner->submit(scene); }
