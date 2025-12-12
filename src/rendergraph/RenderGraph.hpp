#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "RenderGraphBuilder.hpp"
#include "RenderGraphRunner.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "resources/Image.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

class RenderGraph {
private:
	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;

	RenderGraphBuilder m_builder;
	std::optional<RenderGraphRunner> m_runner;

	ResourceIndex m_outputImage = UINT32_MAX;

	std::unordered_map<std::string_view, FeatureIndex> m_features;
	std::unordered_set<FeatureIndex> m_enabledFeatures;
	bool m_graphUpdated = true;

	std::vector<GraphData::TaskData> m_tasks;

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
	) {
		return m_builder.createImage(name, desc, swapchainRatio);
	}
	ResourceIndex createBuffer(
		std::string_view name, ResourceManager::BufferDescription desc
	) {
		return m_builder.createBuffer(name, desc);
	}

	void addTask(
		std::string_view name,
		TaskType type,
		std::vector<ResourceDependency> inputResources,
		std::vector<ResourceDependency> outputResources,
		Task task,
		FeatureIndex feature = 0
	) {
		m_builder.addTask(
			name,
			type,
			std::move(inputResources),
			std::move(outputResources),
			std::move(task),
			feature
		);
	}

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
