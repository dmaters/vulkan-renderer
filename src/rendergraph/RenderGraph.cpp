#include "RenderGraph.hpp"

#include <cassert>
#include <optional>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Swapchain.hpp"
#include "material/Pipeline.hpp"
#include "rendergraph/GraphData.hpp"
#include "rendergraph/RenderGraphBuilder.hpp"
#include "rendergraph/ResourceUsage.hpp"
#include "rendergraph/tasks/Task.hpp"
#include "resources/ResourceManager.hpp"

RenderGraph::RenderGraph(
	Swapchain& swapchain,
	ResourceManager& resourceManager,
	MaterialManager& materialManager
) :
	m_swapchain(swapchain),
	m_resourceManager(resourceManager),
	m_materialManager(materialManager),
	m_builder(m_data) {}

ResourceIndex RenderGraph::createImage(
	std::string name,
	ResourceManager::ImageDescription desc,
	uint8_t swapchainRatio
) {
	ResourceIndex index = m_data.resourceNames.size();
	m_data.images[index] = desc;

	if (swapchainRatio != 0) m_data.swapchainImageRatio[index] = swapchainRatio;

	m_data.resourceNames.push_back(name);
	m_data.finalUsages[index] = ResourceUsage::Type::Undefined;

	return index;
}

ResourceIndex RenderGraph::registerImage(std::string name, ImageHandle image) {
	ResourceIndex index = m_data.resourceNames.size();

	m_data.resourceNames.push_back(name);
	m_data.externalImages[index] = image;
	m_resourceManager.setName(m_data.resourceNames[index], image);
	m_data.finalUsages[index] = ResourceUsage::Type::Undefined;

	return index;
}

ResourceIndex RenderGraph::createDeviceBuffer(
	std::string name, ResourceManager::BufferDescription desc, bool shared
) {
	assert(desc.size > 0);
	ResourceIndex index = m_data.resourceNames.size();
	m_data.buffers[index] = desc;
	m_data.resourceNames.push_back(name);

	if (shared) m_data.sharedBuffers.insert(index);

	return index;
}

ResourceIndex RenderGraph::createHostBuffer(std::string name, uint32_t size) {
	ResourceIndex index = m_data.resourceNames.size();
	m_data.localBufferSizes[index] = size;
	m_data.resourceNames.push_back(name);

	return index;
}

ResourceIndex RenderGraph::registerBuffer(
	std::string name, BufferHandle buffer
) {
	ResourceIndex index = m_data.resourceNames.size();

	m_data.resourceNames.push_back(name);
	m_data.externalBuffers[index] = buffer;
	m_resourceManager.setName(m_data.resourceNames[index], buffer);
	return index;
}

TaskIndex RenderGraph::addTask(
	std::string name,
	TaskType type,
	std::vector<ResourceDependency> inputResources,
	std::vector<ResourceDependency> outputResources,
	Task task
) {
	TaskIndex index = m_data.tasks.size();

	uint32_t offset = m_data.taskDependencies.size();
	uint8_t inputSize = inputResources.size();
	m_data.taskData.push_back(
		{
			.type = type,
			.name = name,
			.inputs = {
		        .offset = offset,
				.count = inputSize,
			},
			.outputs = {
    			.offset = offset + inputSize,
    		    .count = static_cast<uint8_t>(outputResources.size())
			},
		}
	);
	m_data.tasks.push_back(std::move(task));

	m_data.taskDependencies.insert(
		m_data.taskDependencies.end(),
		inputResources.begin(),
		inputResources.end()
	);
	m_data.taskDependencies.insert(
		m_data.taskDependencies.end(),
		outputResources.begin(),
		outputResources.end()
	);

	m_builder.addTask(index, inputResources, outputResources);

	return index;
}

void RenderGraph::submit(const Scene& scene) {
	assert(m_outputImage != UINT32_MAX);

	if (!m_runner.has_value()) {
		m_runner.emplace(
			m_data, m_swapchain, m_resourceManager, m_materialManager
		);
	}
	if (m_graphUpdated) {
		m_graphUpdated = false;
		rendergraph::internal::ExecutionInfo executionInfo =
			m_builder.getTasks(m_outputImage);

		m_data.finalUsages = std::move(executionInfo.finalUsages);

		m_runner->update(m_outputImage, executionInfo);
	}

	m_runner->submit(scene);
}
