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
	m_builder(m_data) {
	addFeatureFlag("baseline");
}

ResourceIndex RenderGraph::createImage(
	std::string_view name,
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

ResourceIndex RenderGraph::registerImage(
	std::string_view name, ImageHandle image
) {
	ResourceIndex index = m_data.resourceNames.size();

	m_data.resourceNames.push_back(name);
	m_data.externalImages[index] = image;
	m_resourceManager.setName(name, image);
	m_data.finalUsages[index] = ResourceUsage::Type::Undefined;

	return index;
}

ResourceIndex RenderGraph::createBuffer(
	std::string_view name, ResourceManager::BufferDescription desc
) {
	ResourceIndex index = m_data.resourceNames.size();
	m_data.buffers[index] = desc;
	m_data.resourceNames.push_back(name);

	return index;
}

ResourceIndex RenderGraph::registerBuffer(
	std::string_view name, BufferHandle buffer
) {
	ResourceIndex index = m_data.resourceNames.size();

	m_data.resourceNames.push_back(name);
	m_data.externalBuffers[index] = buffer;
	m_resourceManager.setName(name, buffer);
	return index;
}

void RenderGraph::addComputePass(
	std::string_view name,
	std::vector<ResourceDependency> inputResources,
	std::vector<ResourceDependency> outputResources,
	ShaderModule modules,
	Task task,
	FeatureIndex feature
) {
	std::vector<vk::DescriptorSetLayoutBinding> materialBindings;

	for (auto& input : inputResources) {
		auto descriptor = ResourceUsage::getDescriptorType(
			input.second, m_data.buffers.contains(input.first)
		);

		if (!descriptor.has_value()) continue;

		materialBindings.push_back(
			{
				.binding = static_cast<uint32_t>(materialBindings.size()),
				.descriptorType = descriptor.value(),
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eAll,
			}
		);
	}
	for (auto& output : outputResources) {
		auto descriptor = getDescriptorType(
			output.second, m_data.buffers.contains(output.first)
		);

		if (!descriptor.has_value()) continue;

		materialBindings.push_back(
			{
				.binding = static_cast<uint32_t>(materialBindings.size()),
				.descriptorType = descriptor.value(),
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eAll,
			}
		);
	}

	m_materialManager.registerComputeMaterial(
		name, modules, std::move(materialBindings)
	);

	addTask(
		name,
		TaskType::Compute,
		std::move(inputResources),
		std::move(outputResources),
		std::move(task),
		feature
	);
}

void RenderGraph::addGraphicPass(
	std::string_view name,
	std::vector<ResourceDependency> inputResources,
	std::vector<ResourceDependency> outputResources,
	GraphicPipelineModules modules,
	GraphicPipelineConfiguration configuration,
	Task task,
	FeatureIndex feature
) {
	std::vector<vk::DescriptorSetLayoutBinding> materialBindings;

	for (auto& input : inputResources) {
		auto descriptor = ResourceUsage::getDescriptorType(
			input.second, m_data.buffers.contains(input.first)
		);

		if (!descriptor.has_value()) continue;

		materialBindings.push_back(
			{
				.binding = static_cast<uint32_t>(materialBindings.size()),
				.descriptorType = descriptor.value(),
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eAll,
			}
		);
	}
	for (auto& output : outputResources) {
		auto descriptor = ResourceUsage::getDescriptorType(
			output.second, m_data.buffers.contains(output.first)
		);

		if (!descriptor.has_value()) continue;

		materialBindings.push_back(
			{
				.binding = static_cast<uint32_t>(materialBindings.size()),
				.descriptorType = descriptor.value(),
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eAll,
			}
		);
	}

	m_materialManager.registerGraphicMaterial(
		name, modules, std::move(materialBindings), configuration
	);

	addTask(
		name,
		TaskType::Graphic,
		std::move(inputResources),
		std::move(outputResources),
		std::move(task),
		feature
	);
}

void RenderGraph::addTask(
	std::string_view name,
	TaskType type,
	std::vector<ResourceDependency> inputResources,
	std::vector<ResourceDependency> outputResources,
	Task task,
	FeatureIndex feature
) {
	TaskIndex index = m_data.tasks.size();

	m_data.resourceNames.push_back(name);
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
			.feature = feature
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

	m_builder.addTask(index, inputResources, outputResources, feature);
}

FeatureIndex RenderGraph::addFeatureFlag(
	std::string_view name, bool defaultValue
) {
	assert(!m_features.contains(name));

	FeatureIndex index = m_features.size();
	m_features[name] = index;
	if (defaultValue) m_enabledFeatures.insert(index);
	return index;
}

void RenderGraph::setFeatureFlag(std::string_view name, bool value) {
	assert(m_features.contains(name));
	FeatureIndex index = m_features[name];
	setFeatureFlag(index, value);
}
void RenderGraph::setFeatureFlag(FeatureIndex index, bool value) {
	m_graphUpdated = true;

	if (!value && m_enabledFeatures.contains(index))
		m_enabledFeatures.erase(index);

	if (value) {
		m_enabledFeatures.insert(index);
	}
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
			m_builder.getTasks(m_outputImage, m_enabledFeatures);

		m_data.finalUsages = std::move(executionInfo.finalUsages);

		m_runner->update(m_outputImage, executionInfo);
	}

	m_runner->submit(scene);
}
