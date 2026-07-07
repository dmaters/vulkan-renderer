#include "RenderGraph.hpp"

#include <cassert>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Swapchain.hpp"
#include "rendergraph/GraphData.hpp"
#include "rendergraph/Task.hpp"
#include "resources/ResourceManager.hpp"

RenderGraph::RenderGraph(
	Swapchain& swapchain,
	ResourceManager& resourceManager,
	MaterialManager& materialManager

) :
	m_swapchain(swapchain),
	m_resourceManager(resourceManager),
	m_materialManager(materialManager) {}

rendergraph::ResourceIndex RenderGraph::registerImage(
	std::string name, ImageHandle image
) {
	rendergraph::ResourceIndex index = m_data.indexer.registerResource(
		rendergraph::internal::ResourceIndexer::ResourceType::Image
	);

	m_data.resourceNames[index] = name;
	m_data.externalImages.push_back({ index, image });

	m_resourceManager.setName(m_data.resourceNames[index], image);

	return index;
}

rendergraph::ResourceIndex RenderGraph::registerBuffer(
	std::string name, BufferHandle buffer
) {
	rendergraph::ResourceIndex index = m_data.indexer.registerResource(
		rendergraph::internal::ResourceIndexer::ResourceType::Buffer
	);

	m_data.resourceNames[index] = name;
	m_data.externalBuffers.push_back({ index, buffer });

	m_resourceManager.setName(m_data.resourceNames[index], buffer);

	return index;
}

TaskIndex RenderGraph::addTask(std::string name, Task task) {
	TaskIndex index = m_data.tasks.size();

	m_data.taskData[index] = {};

	m_data.taskMetadata[index] = {
		.name = name,
	};
	m_data.tasks[index] = task;

	return index;
}
void RenderGraph::update(std::vector<TaskIndex> tasks, const Scene& scene) {
	rendergraph::internal::ExecutionInfo info = build(tasks, scene);
	m_runner.emplace(
		m_data, m_swapchain, m_resourceManager, m_materialManager, info
	);
}
bool RenderGraph::submit(const Scene& scene) { return m_runner->submit(scene); }
