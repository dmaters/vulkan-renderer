
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

	glm::mat3 orientation = glm::mat3(1);
	orientation[0] = glm::vec3(1, 0, 0);
	orientation[1] = glm::vec3(0, 0, 1);
	orientation[2] = glm::vec3(0, 1, 0);
	orientation = glm::rotate_slow(
		glm::mat4(orientation),
		(-glm::pi<float>() / 2.0f) + UI::Data.lightingData.sunAngleRad,
		glm::vec3(1, 0, 0)
	);

	m_currentScene.lights[0].orientation = orientation;

	m_graph.submit(m_currentScene);

	m_currentFrame = (m_currentFrame + 1) % 3;
};

void Renderer::load(const std::filesystem::path& path) {
	SceneLoader loader(m_resourceManager, m_materialManager);
	m_currentScene = loader.load(path);
	UI::Data.sceneData.scenePath = path;
	UI::Data.sceneData.primitiveCount = m_currentScene.primitives.size();

	createRenderGraph(m_currentScene);

	MaterialIndex gbufferMaterial =
		m_materialManager.getMaterialIndex("gbuffer");
	MaterialIndex shadowMapMaterial =
		m_materialManager.getMaterialIndex("shadowmap");

	for (int i = 0; i < m_currentScene.primitives.size(); i++) {
		m_currentScene.buckets[gbufferMaterial].push_back(i);
		m_currentScene.buckets[shadowMapMaterial].push_back(i);
	}

	glm::mat3 orientation = glm::mat3(1);
	orientation[0] = glm::vec3(1, 0, 0);
	orientation[1] = glm::vec3(0, 0, 1);
	orientation[2] = glm::vec3(0, 1, 0);
	orientation = glm::rotate_slow(
		glm::mat4(orientation), (float)glm::radians(-80.0), glm::vec3(1, 0, 0)
	);

	m_currentScene.lights.push_back(
		{
			.position = glm::vec3(0, 0, 600),
			.orientation = orientation,
			.intensity = 25.0f,
		}
	);

	m_currentScene.primitives.push_back(
		{
			.baseVertex = 0,
			.baseIndex = 0,
			.indexCount = 3,
			.size = 1,
		}
	);
	MaterialIndex lighting =
		m_materialManager.getMaterialIndex("lighting_deferred");
	m_currentScene.buckets[lighting].push_back(
		m_currentScene.primitives.size() - 1
	);

	MaterialIndex skybox = m_materialManager.getMaterialIndex("skybox");
	m_currentScene.buckets[skybox].push_back(
		m_currentScene.primitives.size() - 1
	);

	m_currentScene.camera.setFov(70);
}
