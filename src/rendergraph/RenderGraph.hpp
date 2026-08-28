#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

#include "GraphData.hpp"
#include "RenderGraphRunner.hpp"
#include "RenderingConfiguration.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/Task.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

class RenderGraph {
private:
	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;
	const RenderingConfiguration& m_renderingConfiguration;

	rendergraph::internal::GraphData m_data;
	std::optional<rendergraph::internal::RenderGraphRunner> m_runner;

	rendergraph::internal::ExecutionInfo build(
		TaskIndex outputTask, const std::vector<TaskIndex>& optionalTasks, const Scene& scene
	);

public:
	RenderGraph(
		const RenderingConfiguration& renderingConfiguration,
		Swapchain& swapchain,
		ResourceManager& resourceManager,
		MaterialManager& materialManager
	);

	rendergraph::ResourceIndex registerImage(std::string name, ImageHandle handle);
	rendergraph::ResourceIndex registerBuffer(std::string name, BufferHandle handle);

	TaskIndex addTask(std::string name, Task task, auto taskData);

	TaskIndex addTask(std::string name, Task task);

	void update(TaskIndex outputTask, const std::vector<TaskIndex>& optionalTasks, const Scene& scene);
	bool submit(const Scene& scene);
};

TaskIndex RenderGraph::addTask(std::string name, Task task, auto taskData) {
	TaskIndex index = m_data.tasks.size();

	std::vector<std::byte> rawData(sizeof(taskData));
	std::memcpy(rawData.data(), &taskData, sizeof(taskData));

	m_data.taskData[index] = std::move(rawData);

	m_data.taskMetadata[index] = {
		.name = name,
	};
	m_data.tasks[index] = task;

	return index;
}
