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
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "memory/MemoryAllocator.hpp"
#include "rendergraph/tasks/BufferCopy.hpp"
#include "rendergraph/tasks/OpaquePass.hpp"
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

	m_resourceManager->createBuffer(
		"gset_buffer_local",
		{
			.size = sizeof(GlobalResources),
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
			.location = AllocationLocation::Host,
		}
	);

	Buffer& globalBuffer =
		m_resourceManager->getNamedBuffer("gset_buffer_local");
	m_globalData = (GlobalResources*)globalBuffer.allocation.address;

	m_renderGraph = std::make_unique<RenderGraph>(
		m_instance, *m_swapchain, *m_resourceManager
	);
	m_renderGraph->addBuffer(
		"gset_buffer",
		{
			.size = sizeof(GlobalResources),
			.usage = vk::BufferUsageFlagBits::eTransferDst |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
			.location = AllocationLocation::Device,
			.transient = true,
		}
	);
	m_resourceManager->createBuffer(
		"light_buffer",
		{
			.size = (uint32_t)sizeof(Light::uLightData),
			.usage = vk::BufferUsageFlagBits::eTransferDst |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
			.location = AllocationLocation::Device,
			.transient = false,
		}
	);
	m_materialManager =
		std::make_unique<MaterialManager>(m_instance, *m_resourceManager);
}

void Renderer::createRenderGraph() {
	Buffer& globalBuffer =
		m_resourceManager->getNamedBuffer("gset_buffer_local");

	auto copyDescriptorBufferPass = std::make_unique<BufferCopy>(BufferCopy::BufferCopyInfo{
		.origin = {
			.name = "gset_buffer_local",
			.length = globalBuffer.size,
		},
		.destination = {
			.name = "gset_buffer",
			.length = globalBuffer.size,
			
		}
	});

	m_renderGraph->addTask("data_update", std::move(copyDescriptorBufferPass));

	vk::Extent2D resolution = m_swapchain->getResolution();

	m_renderGraph->addImage(
		"main_color",
		ResourceManager::ImageDescription {
			.width = resolution.width,
			.height = resolution.height,
			.format = vk::Format::eB8G8R8A8Unorm,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eTransferSrc |
	                 vk::ImageUsageFlagBits::eTransferDst,

			.transient = true,

		},
		true
	);
	m_renderGraph->addImage(
		"main_depth",
		ResourceManager::ImageDescription {
			.width = resolution.width,
			.height = resolution.height,
			.format = vk::Format::eD16Unorm,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
	                 vk::ImageUsageFlagBits::eTransferDst,
			.transient = true,
		},
		true
	);

	auto opaquePass = std::make_unique<OpaquePass>(
		m_materialManager->getBaseMaterial(), true
	);

	m_renderGraph->addTask("main_pass", std::move(opaquePass));

	m_renderGraph->build();
}

void Renderer::render() {
	vk::Extent2D resolution = m_swapchain->getResolution();
	glm::mat4 proj = glm::perspectiveRH_ZO(
		glm::radians(60.f),
		(float)resolution.width / resolution.height,
		0.1f,
		1000.0f
	);

	proj[1][1] *= -1;
	m_globalData->camera = {
		.view = m_currentScene->getCamera().getViewVector(),
		.projection = proj,
	};

	m_renderGraph->submit(m_currentScene->getPrimitives());
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

	auto& lights = m_currentScene->getLights();

	Light::uLightData lightData = { .count = (uint32_t)lights.size() };
	for (int i = 0; i < lights.size(); i++) {
		const Light& light = lights[i];
		lightData.lights[i] = light.getShaderObject();
	}
	BufferHandle handle =
		m_resourceManager->getNamedBufferHandle("light_buffer");

	std::byte* raw = (std::byte*)&lightData;
	std::vector<std::byte> lightDataRaw(raw, raw + sizeof(Light::uLightData));

	m_resourceManager->copyToBuffer(lightDataRaw, handle);

	createRenderGraph();
}