#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

#include "GraphData.hpp"
#include "RenderGraphRunner.hpp"
#include "RenderingConfiguration.hpp"
#include "Swapchain.hpp"
#include "TaskRegister.hpp"
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
	TaskRegister& m_taskRegister;

	IndexMap<rendergraph::ResourceIndex, ImageHandle> m_externalImages;
	IndexMap<rendergraph::ResourceIndex, BufferHandle> m_externalBuffers;

	std::optional<rendergraph::internal::RenderGraphRunner> m_runner;

	rendergraph::internal::ExecutionInfo build(
		TaskIndex outputTask, const std::vector<TaskIndex>& optionalTasks, const Scene& scene
	);

public:
	RenderGraph(
		const RenderingConfiguration& renderingConfiguration,
		TaskRegister& taskRegister,
		Swapchain& swapchain,
		ResourceManager& resourceManager,
		MaterialManager& materialManager
	);

	void setImage(rendergraph::ResourceIndex, ImageHandle);
	void setBuffer(rendergraph::ResourceIndex, BufferHandle);

	void update(TaskIndex outputTask, const std::vector<TaskIndex>& optionalTasks, const Scene& scene);
	bool submit(const Scene& scene);
};
