
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

Renderer::Renderer(SDL_Window* window) :
	m_instance(Instance::Create(window)),
	m_resourceManager(),
	m_materialManager(m_resourceManager),
	m_graph(m_configuration, Instance::Get().swapchain, m_resourceManager, m_materialManager) {
	if (window == nullptr) return;

	m_graphicsQueue = m_instance.device.getQueue(m_instance.queueFamiliesIndices.graphicsIndex, 0);
	m_presentQueue = m_instance.device.getQueue(m_instance.queueFamiliesIndices.presentIndex, 0);

	std::vector<ResourceManager::BufferDescription> dummyBuffers(5);

	for (auto& buffer : dummyBuffers) {
		buffer = { 1, vk::BufferUsageFlagBits::eTransferSrc };
	}

	m_resourceManager.createResources({}, dummyBuffers, ResourceManager::MemoryLocation::Device);

	glm::mat3 orientation = glm::mat3(1);
	orientation[0] = glm::vec3(1, 0, 0);
	orientation[1] = glm::vec3(0, 0, 1);
	orientation[2] = glm::vec3(0, 1, 0);
	orientation = glm::rotate_slow(glm::mat4(orientation), (float)glm::radians(-80.0), glm::vec3(1, 0, 0));

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
		}
	);

	auto swapchainResolution = Instance::Get().swapchain.getResolution();
	m_configuration.resolution = {
		swapchainResolution.width,
		swapchainResolution.height,
	};

	m_currentScene.camera.fov = {
		70 * m_configuration.resolution.x / m_configuration.resolution.y,
		70,
	};
	m_optionalPasses = createRenderGraph(m_currentScene);
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

	m_currentScene.lights[0].orientation = orientation;

	bool res = m_graph.submit(m_currentScene);

	if (!res) {
		Instance::Get().swapchain.rebuild();
		auto swapchainResolution = Instance::Get().swapchain.getResolution();

		m_configuration.resolution = {
			swapchainResolution.width,
			swapchainResolution.height,
		};
		m_graph.update(m_optionalPasses.back(), m_optionalPasses, m_currentScene);
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
	m_graph.update(m_optionalPasses.back(), m_optionalPasses, m_currentScene);
}

void Renderer::load(const std::filesystem::path& path) {
	SceneLoader loader(m_resourceManager, m_materialManager);
	m_currentScene = loader.load(path);

	MaterialIndex lighting = m_materialManager.getMaterialIndex("lighting_deferred");
	m_currentScene.buckets[lighting].push_back(m_currentScene.primitives.size() - 1);

	MaterialIndex skybox = m_materialManager.getMaterialIndex("skybox");
	m_currentScene.buckets[skybox].push_back(m_currentScene.primitives.size() - 1);

	MaterialIndex fxaa = m_materialManager.getMaterialIndex("fxaa");
	m_currentScene.buckets[fxaa].push_back(m_currentScene.primitives.size() - 1);

	UI::Data.sceneData.scenePath = path.string();
	UI::Data.sceneData.primitiveCount = m_currentScene.primitives.size();
}
