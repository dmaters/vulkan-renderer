
#include "Renderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Instance.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"
#include "ui/UI.hpp"

void Renderer::reloadGraphBuffers(
	ResourceManager::AllocationIndex allocation,
	SceneManager::SharedBufferContainer<rendergraph::ResourceIndex> staticResources,
	ResourceManager& resourceManager,
	RenderGraph& rendergraph
) {
	auto sceneBuffers = SceneManager::SharedBufferContainer<BufferHandle>(resourceManager.getBuffers(allocation));

	rendergraph.setBuffer(staticResources.vertices, sceneBuffers.vertices);
	rendergraph.setBuffer(staticResources.vertexAttributes, sceneBuffers.vertexAttributes);
	rendergraph.setBuffer(staticResources.indices, sceneBuffers.indices);
	rendergraph.setBuffer(staticResources.transforms, sceneBuffers.transforms);
	rendergraph.setBuffer(staticResources.materialData, sceneBuffers.materialData);
	rendergraph.setBuffer(staticResources.primitiveData, sceneBuffers.primitiveData);
	rendergraph.setBuffer(staticResources.tlas, sceneBuffers.tlas);
}

void updateGraph(TaskRegister& taskRegister, RenderGraph& graph, Scene& scene) {
	std::vector<TaskIndex> enabledFeatures;

	for (auto [task, enabled] : UI::Data.renderingData.enabledTasks.items()) {
		if (enabled) enabledFeatures.push_back(task);
	}
	TaskIndex ui = taskRegister.getTasks().tasks.topIndex().first;
	graph.update(ui, enabledFeatures, scene);
}

Renderer::Renderer(SDL_Window* window) :
	m_instance(Instance::Create(window)),
	m_resourceManager(),
	m_materialManager(m_resourceManager),
	m_sceneManager(m_resourceManager, m_materialManager) {
	if (window == nullptr) return;

	m_graphicsQueue = m_instance.device.getQueue(m_instance.queueFamiliesIndices.graphicsIndex, 0);
	m_presentQueue = m_instance.device.getQueue(m_instance.queueFamiliesIndices.presentIndex, 0);

	auto swapchainResolution = Instance::Get().swapchain.getResolution();
	m_configuration.resolution = {
		swapchainResolution.width,
		swapchainResolution.height,
	};

	m_scene = m_sceneManager.getScene();
	glm::mat3 orientation = glm::mat3(1);
	orientation[0] = glm::vec3(1, 0, 0);
	orientation[1] = glm::vec3(0, 0, 1);
	orientation[2] = glm::vec3(0, 1, 0);
	orientation = glm::rotate_slow(glm::mat4(orientation), (float)glm::radians(-80.0), glm::vec3(1, 0, 0));

	m_scene.light = {
		.position = glm::vec3(0, 0, 600),
		.orientation = orientation,
		.intensity = 25.0f,
	};

	m_scene.camera.position = glm::vec3(0, 0, 150);

	m_scene.camera.fov = {
		70 * m_configuration.resolution.x / m_configuration.resolution.y,
		70,
	};

	registerPasses();

	UI::Data.renderingData.featureRegister = m_featureRegister;
	m_graph.emplace(m_configuration, m_taskRegister, Instance::Get().swapchain, m_resourceManager, m_materialManager);
	reloadGraphBuffers(m_scene.allocation, m_sharedSceneBuffers, m_resourceManager, *m_graph);
	updateGraph(m_taskRegister, *m_graph, m_scene);
}

void Renderer::render() {
	m_materialManager.update();
	auto swapchainResolution = Instance::Get().swapchain.getResolution();

	if (swapchainResolution.width == 0 && swapchainResolution.height == 0) {
		Instance::Get().swapchain.rebuild();
		return;
	}

	m_configuration.resolution = {
		swapchainResolution.width,
		swapchainResolution.height,
	};

	glm::mat3 orientation =
		glm::mat3(glm::rotate(glm::mat4(1.0f), UI::Data.lightingData.sunAngleRad, glm::vec3(1, 0, 0)));

	m_scene.light.orientation = orientation;

	bool res = m_graph->submit(m_scene);

	if (m_loadingScene) {
		std::size_t loadedCount = m_sceneManager.sync();
		UI::Data.sceneData.resourceLoadedCount = loadedCount;

		if (loadedCount == m_resourceCount) {
			Instance::Get().device.waitIdle();
			m_loadingScene = false;
			auto scene = m_sceneManager.getScene();
			auto camera = m_scene.camera;
			auto light = m_scene.light;
			m_scene = scene;
			m_scene.camera = camera;
			m_scene.light = light;

			reloadGraphBuffers(m_scene.allocation, m_sharedSceneBuffers, m_resourceManager, *m_graph);
			updateGraph(m_taskRegister, *m_graph, m_scene);

			UI::Data.sceneData.primitiveCount = m_scene.primitives.size();
		}
	}

	if (!res) {
		Instance::Get().swapchain.rebuild();
		auto swapchainResolution = Instance::Get().swapchain.getResolution();

		m_configuration.resolution = {
			swapchainResolution.width,
			swapchainResolution.height,
		};
		updateGraph(m_taskRegister, *m_graph, m_scene);
	}

	if (UI::Data.renderingData.updated) {
		updateGraph(m_taskRegister, *m_graph, m_scene);
	}
};

void Renderer::setResolution(int width, int height) {
	auto& swapchain = Instance::Get().swapchain;

	swapchain.setResolutionHint({ (uint32_t)width, (uint32_t)height });
	swapchain.rebuild();

	m_configuration.resolution = {
		width,
		height,
	};
	updateGraph(m_taskRegister, *m_graph, m_scene);
}

void Renderer::load(const std::filesystem::path& path) {
	Instance::Get().device.waitIdle();
	m_loadingScene = true;
	m_resourceCount = m_sceneManager.loadAsync(path);

	UI::Data.sceneData.scenePath = path.string();
	UI::Data.sceneData.resourceCount = m_resourceCount;
}
