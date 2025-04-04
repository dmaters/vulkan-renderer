#include "Renderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <mfidl.h>
#include <vulkan/vulkan_core.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "Rendergraph/RenderGraph.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "memory/MemoryAllocator.hpp"
#include "rendergraph/tasks/BufferCopy.hpp"
#include "rendergraph/tasks/ImageCopy.hpp"
#include "rendergraph/tasks/OpaquePass.hpp"
#include "resources/ResourceManager.hpp"
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
	createSwapchain();
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
	m_resourceManager->createBuffer(
		"gset_buffer",
		{
			.size = sizeof(GlobalResources),
			.usage = vk::BufferUsageFlagBits::eTransferDst |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
			.location = AllocationLocation::Device,
			.transient = true,
		}
	);
	Buffer& globalBuffer =
		m_resourceManager->getNamedBuffer("gset_buffer_local");
	m_globalData = (GlobalResources*)globalBuffer.allocation.address;

	m_materialManager =
		std::make_unique<MaterialManager>(m_instance, *m_resourceManager);
	m_renderGraph = std::make_unique<RenderGraph>(
		m_instance, *m_swapchain, *m_resourceManager
	);
}

void Renderer::createSwapchain() {
	vk::CommandPoolCreateInfo info;
	info.queueFamilyIndex = m_instance.queueFamiliesIndices.graphicsIndex;

	auto formats =
		m_instance.physicalDevice.getSurfaceFormatsKHR(m_instance.surface);

	Swapchain::SwapchainCreateInfo swapchainInfo {
		.width = 800,
		.height = 600,
		.graphicsFamilyIndex = m_instance.queueFamiliesIndices.graphicsIndex,
		.surface = m_instance.surface,
		.format = { .format = formats[0].format,
                   .colorSpace = formats[0].colorSpace },
	};

	m_swapchain = std::make_unique<Swapchain>(m_instance.device, swapchainInfo);
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

	m_resourceManager->createImage(
		"main_color",
		ResourceManager::ImageDescription {
			.width = 800,
			.height = 600,
			.format = vk::Format::eB8G8R8A8Unorm,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eTransferSrc |
	                 vk::ImageUsageFlagBits::eTransferDst,

			.transient = true,

		}
	);
	m_resourceManager->createImage(
		"main_depth",
		ResourceManager::ImageDescription {
			.width = 800,
			.height = 600,
			.format = vk::Format::eD16Unorm,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
	                 vk::ImageUsageFlagBits::eTransferDst,
			.transient = true,
		}
	);

	auto opaquePass = std::make_unique<OpaquePass>(
		m_materialManager->getBaseMaterial(), true
	);

	m_renderGraph->addTask("main_pass", std::move(opaquePass));

	auto imageCopyTask = std::make_unique<ImageCopy>("main_color", "result");
	m_renderGraph->addTask("result_copy", std::move(imageCopyTask));
}

void Renderer::render() {
	glm::mat4 proj =
		glm::perspectiveRH_ZO(glm::radians(60.f), 800.f / 600.f, 0.1f, 1000.0f);

	proj[1][1] *= -1;
	m_globalData->camera = {
		.view = m_camera.getViewVector(),
		.projection = proj,
	};

	m_renderGraph->submit(m_currentScene->getPrimitives());
};

void Renderer::load(const std::filesystem::path& path) {
	SceneLoader loader(*m_resourceManager, *m_materialManager);
	m_currentScene = std::make_unique<Scene>(loader.load(path));

	createRenderGraph();
}