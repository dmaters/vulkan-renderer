#include "Renderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <glm/ext/matrix_clip_space.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "Rendergraph/RenderGraph.hpp"
#include "Rendergraph/RenderGraphBuilder.hpp"
#include "Swapchain.hpp"
#include "material/MaterialDefinitions.hpp"
#include "material/MaterialManager.hpp"
#include "material/MaterialManager_impl.hpp"
#include "memory/MemoryAllocator.hpp"
#include "rendergraph/tasks/ImageCopy.hpp"
#include "rendergraph/tasks/RenderPass.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Light.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp"

Renderer::Renderer(SDL_Window* window) {
	if (window == nullptr) return;

	m_instance = Instance::Create(window);

	m_graphicsQueue = m_instance.device.getQueue(
		m_instance.queueFamiliesIndices.graphicsIndex, 0
	);

	m_presentQueue = m_instance.device.getQueue(
		m_instance.queueFamiliesIndices.presentIndex, 0
	);
	m_swapchain = std::make_unique<Swapchain>(m_instance);

	m_memoryAllocator = std::make_unique<MemoryAllocator>(m_instance);
	m_resourceManager =
		std::make_unique<ResourceManager>(m_instance, *m_memoryAllocator);

	m_materialManager =
		std::make_unique<MaterialManager>(m_instance, *m_resourceManager);

	m_renderGraph = std::make_unique<RenderGraph>(
		m_instance, *m_swapchain, *m_resourceManager, *m_materialManager
	);
}

void Renderer::createRenderGraph() {
	vk::Extent2D resolution = m_swapchain->getResolution();

	for (const auto& [name, desc] : ResourceManager::_defaultNamedImageData) {
		auto it = ResourceManager::_swapchainRatio.find(name);
		int8_t ratio =
			(it != ResourceManager::_swapchainRatio.end()) ? it->second : 0;
		m_renderGraph->addImage(name, desc, ratio);
	}

	for (const auto& [name, desc] : ResourceManager::_defaultNamedBufferData) {
		m_renderGraph->addBuffer(name, desc);
	}
	RenderGraphBuilder builder;

	for (const auto& [index, metadata] :
	     m_materialManager->getMaterialMetadatas()) {
		builder.addTask(RenderPass(index, metadata.namedResourceDependencies));
	}

	builder.addTask(ImageCopy("main_color", "result"));

	GraphData data = builder.build();
	m_renderGraph->build(data);
}

void Renderer::render() {
	m_materialManager->update(m_currentFrame);
	vk::Extent2D resolution = m_swapchain->getResolution();
	glm::mat4 proj = glm::perspectiveRH_ZO(
		glm::radians(60.f),
		(float)resolution.width / resolution.height,
		0.1f,
		1000.0f
	);

	proj[1][1] *= -1;

	glm::mat4 view = m_currentScene->getCamera().getViewVector();

	m_materialManager->updateGlobalBuffer<MaterialDefinitions::ViewProjection>(
		"view_projection",
		[proj, view](MaterialDefinitions::ViewProjection& viewProj) {
			viewProj.view = view;
			viewProj.projection = proj;
		}
	);

	m_renderGraph->submit(m_currentScene->getPrimitives());

	m_currentFrame = (m_currentFrame + 1) % 3;
};

void Renderer::load(const std::filesystem::path& path) {
	SceneLoader loader(*m_resourceManager, *m_materialManager);
	m_currentScene = std::make_unique<Scene>(loader.load(path));
	glm::mat3 orientation = glm::mat3(1);
	orientation[1] = glm::vec3(0, 0, 1);
	orientation[2] = glm::vec3(0, -1, 0);

	m_currentScene->addLight({
		.position = glm::vec3(0, 150, 0),
		.orientation = orientation,
		.intensity = 0.2,
	});

	auto lights = m_currentScene->getLights();

	m_materialManager->updateGlobalBuffer<MaterialDefinitions::Lights>(
		"light_buffer",

		[lights](MaterialDefinitions::Lights& lightUBO) {
			lightUBO.count = lights.size();
			for (int i = 0; i < lights.size(); i++) {
				lightUBO.lights[i] = lights[i].getShaderObject();
			}
		}
	);

	createRenderGraph();
}