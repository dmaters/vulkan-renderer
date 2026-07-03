#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

#include "GraphData.hpp"
#include "RenderGraphRunner.hpp"
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

	rendergraph::internal::GraphData m_data;
	std::optional<rendergraph::internal::RenderGraphRunner> m_runner;

	rendergraph::internal::ExecutionInfo build(
		const std::vector<TaskIndex>& taskList, const Scene& scene
	);

public:
	RenderGraph(
		Swapchain& swapchain,
		ResourceManager& resourceManager,
		MaterialManager& materialManager
	);

	rendergraph::ResourceIndex createImage(
		std::string name, ResourceManager::ImageDescription desc
	);

	rendergraph::ResourceIndex createDeviceBuffer(
		std::string name, ResourceManager::BufferDescription desc
	);

	rendergraph::ResourceIndex registerImage(
		std::string name, ImageHandle handle
	);
	rendergraph::ResourceIndex registerBuffer(
		std::string name, BufferHandle handle
	);

	TaskIndex addTask(
		std::string name, Task::Type type, Task task, auto taskData
	);

	TaskIndex addTask(std::string name, Task::Type type, Task task);

	void update(std::vector<TaskIndex> tasks, const Scene& scene);
	void submit(const Scene& scene);
};

// Definisci task come 3 funzioni
//  TTaskdata passato apparte funziona come coso
//  Definisci funzione template, che dato l'indice del task prende i dati di
//  quel task
