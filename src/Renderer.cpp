
#include "Renderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "Swapchain.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp"
#include "ui/UI.hpp"

Renderer::Renderer(SDL_Window* window) :
	m_instance(Instance::Create(window)),
	m_resourceManager(),
	m_materialManager(m_resourceManager),
	m_graph(Instance::Get().swapchain, m_resourceManager, m_materialManager) {
	if (window == nullptr) return;

	m_graphicsQueue = m_instance.device.getQueue(
		m_instance.queueFamiliesIndices.graphicsIndex, 0
	);

	m_presentQueue = m_instance.device.getQueue(
		m_instance.queueFamiliesIndices.presentIndex, 0
	);

	UI::Setup();
}

void Renderer::render() {
	m_materialManager.update();
	vk::Extent2D resolution = Instance::Get().swapchain.getResolution();

	if (resolution == vk::Extent2D(0)) {
		Instance::Get().swapchain.rebuild();
		return;
	}

	m_currentScene.camera.setResolution(
		{
			resolution.width,
			resolution.height,
		}
	);

	glm::mat3 orientation = glm::mat3(
		glm::rotate(
			glm::mat4(1.0f),
			UI::Data.lightingData.sunAngleRad,
			glm::vec3(1, 0, 0)
		)
	);

	m_currentScene.lights[0].orientation = orientation;

	m_graph.submit(m_currentScene);
};

void Renderer::load(const std::filesystem::path& path) {
	SceneLoader loader(m_resourceManager, m_materialManager);
	m_currentScene = loader.load(path);
	UI::Data.sceneData.scenePath = path.string();
	UI::Data.sceneData.primitiveCount = m_currentScene.primitives.size();

	createRenderGraph(m_currentScene);
}
