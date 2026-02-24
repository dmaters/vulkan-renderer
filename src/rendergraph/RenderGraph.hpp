#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "GraphData.hpp"
#include "RenderGraphBuilder.hpp"
#include "RenderGraphRunner.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "material/Pipeline.hpp"
#include "rendergraph/tasks/Task.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

class RenderGraph {
private:
	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;

	rendergraph::internal::GraphData m_data;
	rendergraph::internal::RenderGraphBuilder m_builder;
	std::optional<rendergraph::internal::RenderGraphRunner> m_runner;

	ResourceIndex m_outputImage = UINT32_MAX;

	std::unordered_map<std::string_view, FeatureIndex> m_features;
	std::unordered_set<FeatureIndex> m_enabledFeatures;
	bool m_graphUpdated = true;

public:
	RenderGraph(
		Swapchain& swapchain,
		ResourceManager& resourceManager,
		MaterialManager& materialManager
	);

	ResourceIndex createImage(
		std::string_view name,
		ResourceManager::ImageDescription desc,
		uint8_t swapchainRatio = 0
	);

	ResourceIndex createDeviceBuffer(
		std::string_view name, ResourceManager::BufferDescription desc
	);

	ResourceIndex createHostBuffer(std::string_view name, uint32_t size);

	ResourceIndex registerImage(std::string_view name, ImageHandle handle);
	ResourceIndex registerBuffer(std::string_view name, BufferHandle handle);

	void addTask(
		std::string_view name,
		TaskType type,
		std::vector<ResourceDependency> inputResources,
		std::vector<ResourceDependency> outputResources,
		Task task,
		FeatureIndex feature = 0
	);

	void setOutputImage(ResourceIndex image) {
		m_outputImage = image;
		m_graphUpdated = true;
	}

	FeatureIndex addFeatureFlag(
		std::string_view name, bool defaultValue = true
	);

	void setFeatureFlag(std::string_view name, bool value);
	void setFeatureFlag(FeatureIndex index, bool value);

	void submit(const Scene& scene);
};
