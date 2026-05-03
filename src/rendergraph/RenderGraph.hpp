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

	bool m_graphUpdated = true;

public:
	RenderGraph(
		Swapchain& swapchain,
		ResourceManager& resourceManager,
		MaterialManager& materialManager
	);

	ResourceIndex createImage(
		std::string name,
		ResourceManager::ImageDescription desc,
		uint8_t swapchainRatio = 0
	);

	ResourceIndex createDeviceBuffer(
		std::string name,
		ResourceManager::BufferDescription desc,
		bool shared = false
	);

	ResourceIndex createHostBuffer(std::string name, uint32_t size);

	ResourceIndex registerImage(std::string name, ImageHandle handle);
	ResourceIndex registerBuffer(std::string name, BufferHandle handle);

	TaskIndex addTask(
		std::string name,
		TaskType type,
		std::vector<ResourceDependency> inputResources,
		std::vector<ResourceDependency> outputResources,
		Task task
	);

	void setOutputImage(ResourceIndex image) {
		m_outputImage = image;
		m_graphUpdated = true;
	}

	void submit(const Scene& scene);
};
