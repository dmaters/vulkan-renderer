
#include "Renderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <glm/ext/matrix_clip_space.hpp>
#include <iostream>
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
#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/tasks/ImageCopy.hpp"
#include "rendergraph/tasks/RenderPass.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Light.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp"

Renderer::Renderer(SDL_Window* window) :
	m_instance(Instance::Create(window)),
	m_resourceManager(),
	m_materialManager(m_resourceManager),
	m_swapchain(),
	m_renderGraph(m_swapchain, m_resourceManager, m_materialManager) {
	if (window == nullptr) return;

	m_graphicsQueue = m_instance.device.getQueue(
		m_instance.queueFamiliesIndices.graphicsIndex, 0
	);

	m_presentQueue = m_instance.device.getQueue(
		m_instance.queueFamiliesIndices.presentIndex, 0
	);
}

void Renderer::createRenderGraph() {
	for (const auto& [name, desc] : ResourceManager::_defaultNamedImageData) {
		auto it = ResourceManager::_swapchainRatio.find(name);
		int8_t ratio =
			(it != ResourceManager::_swapchainRatio.end()) ? it->second : 0;
		m_renderGraph.addImage(name, desc, ratio);
	}

	for (const auto& [name, desc] : ResourceManager::_defaultNamedBufferData) {
		m_renderGraph.addBuffer(name, desc);
	}
	RenderGraphBuilder builder;

	for (const auto& [index, metadata] :
	     m_materialManager.getMaterialMetadatas()) {
		builder.addTask(RenderPass(index, metadata.namedResourceDependencies));
	}

	GraphData data = builder.build();
	m_renderGraph.build(data);
}

void Renderer::render() {
	m_materialManager.update(m_currentFrame);
	vk::Extent2D resolution = m_swapchain.getResolution();

	if (resolution == vk::Extent2D(0)) {
		m_swapchain.rebuild();
		return;
	}
	m_currentScene.camera.setResolution({
		resolution.width,
		resolution.height,
	});

	m_renderGraph.submit(m_currentScene);

	m_currentFrame = (m_currentFrame + 1) % 3;
};

void Renderer::load(const std::filesystem::path& path) {
	SceneLoader loader(m_resourceManager, m_materialManager);
	m_currentScene = loader.load(path);

	glm::mat3 orientation = glm::mat3(1);
	orientation[1] = glm::vec3(0, 0, 1);
	orientation[2] = glm::vec3(0, 1, 0);
	orientation[0] = glm::vec3(1, 0, 0);
	m_currentScene.lights.push_back({
		.position = glm::vec3(0, 150, 0),
		.orientation = orientation,
		.intensity = 20,
	});

	const auto& lights = m_currentScene.lights;

	m_resourceManager.updateBufferSync<MaterialDefinitions::Lights>(
		m_resourceManager.getNamedBufferIndex("light_buffer"),
		[lights](MaterialDefinitions::Lights& lightUBO) {
			lightUBO.count = lights.size();
			lightUBO.directLightIndex = 0;
			for (int i = 0; i < lights.size(); i++) {
				lightUBO.lights[i] = lights[i].getShaderObject();
			}
		}
	);

	m_resourceManager.updateBufferSync<MaterialDefinitions::EnvironmentData>(
		m_resourceManager.getNamedBufferIndex("environment_data"),
		[size =
	         m_currentScene.size](MaterialDefinitions::EnvironmentData& envData
	    ) {
			envData.sceneSize = size;
			envData.environmentColor = glm::vec3(0.04, 0.02, 0.1);
			// envData.environmentColor = glm::vec3(1);
		}
	);

	MaterialIndex lighting =
		m_materialManager.getMaterialIndex("lighting_deferred");
	m_currentScene.primitives.push_back({
		.baseVertex = 0,
		.baseIndex = 0,
		.indexCount = 3,
		.materials = { {
			lighting,
		} },
	});
	createRenderGraph();
}
