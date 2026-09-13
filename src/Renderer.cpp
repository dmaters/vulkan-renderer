
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
#include "scene/SceneLoader.hpp"
#include "ui/UI.hpp"

void Renderer::reloadGraphBuffers(
	ResourceManager::AllocationIndex allocation,
	StaticResources staticResources,
	ResourceManager& resourceManager,
	RenderGraph& rendergraph
) {
	auto sceneBuffers = resourceManager.getBuffers(allocation);

	rendergraph.setBuffer(staticResources.vertexBuffer, sceneBuffers[(int)SceneManager::SceneBufferType::Vertex]);
	rendergraph.setBuffer(
		staticResources.vertexAttributeBuffer, sceneBuffers[(int)SceneManager::SceneBufferType::VertexAttribute]
	);
	rendergraph.setBuffer(staticResources.indexBuffer, sceneBuffers[(int)SceneManager::SceneBufferType::Index]);
	rendergraph.setBuffer(staticResources.transforms, sceneBuffers[(int)SceneManager::SceneBufferType::Transforms]);
	rendergraph.setBuffer(
		staticResources.pbrMaterialData, sceneBuffers[(int)SceneManager::SceneBufferType::MaterialData]
	);
	rendergraph.setBuffer(
		staticResources.primitiveData, sceneBuffers[(int)SceneManager::SceneBufferType::PrimitiveData]
	);
}

Renderer::Renderer(SDL_Window* window) :
	m_instance(Instance::Create(window)),
	m_resourceManager(),
	m_materialManager(m_resourceManager),
	m_graph(m_configuration, Instance::Get().swapchain, m_resourceManager, m_materialManager),
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

	m_passes = createRenderGraph();
	reloadGraphBuffers(m_scene.allocation, m_staticResources, m_resourceManager, m_graph);

	m_graph.update(m_passes.ui, m_passes.optionalPasses, m_scene);
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

	bool res = m_graph.submit(m_scene);

	if (m_loadingScene) {
		std::size_t loadedCount = m_sceneManager.sync();
		if (loadedCount == m_resourceCount) {
			Instance::Get().device.waitIdle();
			m_loadingScene = false;
			auto scene = m_sceneManager.getScene();

			m_scene.primitives = scene.primitives;
			m_scene.materialHints = scene.materialHints;
			m_scene.primitiveBounds = scene.primitiveBounds;
			m_scene.allocation = scene.allocation;
			reloadGraphBuffers(m_scene.allocation, m_staticResources, m_resourceManager, m_graph);

			m_graph.update(m_passes.optionalPasses.back(), m_passes.optionalPasses, m_scene);
		}
	}

	if (!res) {
		Instance::Get().swapchain.rebuild();
		auto swapchainResolution = Instance::Get().swapchain.getResolution();

		m_configuration.resolution = {
			swapchainResolution.width,
			swapchainResolution.height,
		};

		m_graph.update(m_passes.optionalPasses.back(), m_passes.optionalPasses, m_scene);
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

	m_graph.update(m_passes.optionalPasses.back(), m_passes.optionalPasses, m_scene);
}

void Renderer::load(const std::filesystem::path& path) {
	Instance::Get().device.waitIdle();
	m_loadingScene = true;
	m_resourceCount = m_sceneManager.loadAsync(path);
}
