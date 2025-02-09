#include "Renderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <mfidl.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Scene.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "resources/Image.hpp"
#include "resources/MemoryAllocator.hpp"

Renderer::Renderer(SDL_Window* window) {
	if (window == nullptr) return;

#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)

	VULKAN_HPP_DEFAULT_DISPATCHER.init();
#endif

	vkb::InstanceBuilder builder;

	builder.set_app_name("Renderer")
		.request_validation_layers()
		//.enable_layer("VK_LAYER_LUNARG_api_dump")
		.require_api_version(vk::ApiVersion13)
		.use_default_debug_messenger();

	auto inst_ret = builder.build();
	if (!inst_ret) {
		std::cerr << "Failed to create Vulkan instance. Error: "
				  << inst_ret.error().message() << "\n";
	}
	vkb::Instance vkb_inst = inst_ret.value();
	auto systemInfo = vkb::SystemInfo::get_system_info();

	VkSurfaceKHR surface;
	SDL_Vulkan_CreateSurface(window, vkb_inst.instance, nullptr, &surface);
	m_surface = surface;
	createDevice(vkb_inst);
	createSwapchain();

	m_allocator = std::make_unique<MemoryAllocator>(m_instance);
	m_materialManager = std::make_unique<MaterialManager>(m_instance);
}

void Renderer::createSwapchain() {
	vk::CommandPoolCreateInfo info;
	info.queueFamilyIndex = m_instance.queueFamiliesIndices.graphicsIndex;

	auto formats = m_instance.physicalDevice.getSurfaceFormatsKHR(m_surface);

	Swapchain::SwapchainCreateInfo swapchainInfo {
		.width = 800,
		.height = 600,
		.graphicsFamilyIndex = m_instance.queueFamiliesIndices.graphicsIndex,
		.surface = m_surface,
		.format = { .format = formats[0].format,
                   .colorSpace = formats[0].colorSpace },
	};

	m_swapchain = std::make_unique<Swapchain>(m_instance.device, swapchainInfo);
}

void Renderer::createDevice(vkb::Instance& instance) {
	vkb::PhysicalDeviceSelector selector { instance };

	auto extensions = { "VK_KHR_swapchain",
		                "VK_KHR_dynamic_rendering",
		                "VK_KHR_depth_stencil_resolve",
		                "VK_KHR_create_renderpass2",
		                "VK_KHR_multiview",
		                "VK_KHR_maintenance2",
		                "VK_KHR_synchronization2" };

	VkPhysicalDeviceVulkan13Features features {};
	features.dynamicRendering = true;
	features.synchronization2 = true;

	auto phys_ret =
		selector.set_surface(m_surface)
			.set_minimum_version(1, 3)  // require a vulkan 1.1 capable device
			.require_dedicated_transfer_queue()
			.set_required_features_13(features)
			.add_required_extensions(extensions)
			.select();

	if (!phys_ret) {
		std::cerr << "Failed to select Vulkan Physical Device. Error: "
				  << phys_ret.error().message() << "\n";
	}

	vkb::DeviceBuilder device_builder { phys_ret.value() };
	// automatically propagate needed data from instance & physical device
	auto dev_ret = device_builder.build();
	if (!dev_ret) {
		std::cerr << "Failed to create Vulkan device. Error: "
				  << dev_ret.error().message() << "\n";
	}
	vkb::Device vkb_device = dev_ret.value();

	m_graphicsQueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
	m_presentQueue = vkb_device.get_queue(vkb::QueueType::present).value();
	m_instance = {
		.device = vkb_device.device,
		.physicalDevice = phys_ret.value().physical_device,
		.instance = instance.instance,
		.queueFamiliesIndices = {
			.graphicsIndex = vkb_device.get_queue_index(vkb::QueueType::graphics).value(),
			.transferIndex = vkb_device.get_queue_index(vkb::QueueType::transfer).value(),
			.presentIndex = vkb_device.get_queue_index(vkb::QueueType::present).value(),
		}
	};
}

void Renderer::render() {
	const Frame& frame = m_swapchain->getNextFrame();

	m_instance.device.waitForFences({ frame.fence }, vk::True, 500);
	m_instance.device.resetFences(frame.fence);

	vk::AcquireNextImageInfoKHR acquireInfo;
	acquireInfo.swapchain = m_swapchain->getSwapchain();
	acquireInfo.timeout = 1500;
	acquireInfo.semaphore = frame.imageAvailable;
	acquireInfo.fence = nullptr;
	acquireInfo.deviceMask = 1;

	auto nextImageRes = m_instance.device.acquireNextImage2KHR(acquireInfo);
	if (nextImageRes.result != vk::Result::eSuccess) throw;

	uint32_t imageIndex = nextImageRes.value;
	Image& currentImage = m_swapchain->getImage(imageIndex);

	vk::CommandBufferAllocateInfo commandInfo;

	commandInfo.commandBufferCount = 1;
	commandInfo.commandPool = frame.commandPool;
	commandInfo.level = vk::CommandBufferLevel::ePrimary;

	auto commandBuffer =
		m_instance.device.allocateCommandBuffers(commandInfo)[0];

	vk::CommandBufferBeginInfo beginInfo;
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	commandBuffer.begin(beginInfo);

	if (currentImage.layout == vk::ImageLayout::eUndefined)
		currentImage.layoutTransition(
			commandBuffer, vk::ImageLayout::eColorAttachmentOptimal
		);
	else
		currentImage.layoutTransition(
			commandBuffer, vk::ImageLayout::eColorAttachmentOptimal

		);
	currentImage.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::RenderingInfoKHR renderingInfo;

	renderingInfo.flags = {};
	renderingInfo.renderArea = vk::Rect2D({ 0, 0 }, { 800, 600 });
	renderingInfo.layerCount = 1;
	renderingInfo.viewMask = 0;
	renderingInfo.colorAttachmentCount = 1;

	vk::RenderingAttachmentInfo colorAttachment;
	colorAttachment.imageView = currentImage.view;
	colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;

	renderingInfo.pColorAttachments = { &colorAttachment };
	renderingInfo.pDepthAttachment = {};
	renderingInfo.pStencilAttachment = {};

	commandBuffer.beginRendering(renderingInfo);
	commandBuffer.setScissor(
		0,
		{
			vk::Rect2D { { 0, 0 }, { 800, 600 } }
    }
	);

	commandBuffer.setViewport(
		0,
		{
			vk::Viewport { 0, 0, 800, 600 }
    }
	);

	commandBuffer.clearAttachments(
		{
			vk::ClearAttachment {
								 .aspectMask = vk::ImageAspectFlagBits::eColor,
								 .colorAttachment = 0,
								 .clearValue =
					vk::ClearValue {
						.color =
							vk::ClearColorValue {
								.int32 = vk::ArrayWrapper1D<int32_t, 4>(
									{ 0, 0, 0, 0 }
								) } },
								 },
    },
		{ vk::ClearRect { .rect = { { 0, 0 }, { 800, 600 } },
	                      .layerCount = 1 } }
	);

	std::array<glm::mat4, 2> viewProjection = {
		m_camera.getViewVector(),
		glm::perspective(glm::radians(60.f), 800.f / 600.f, 0.1f, 1000.0f)
	};

	std::vector<unsigned char> cameraData(
		(unsigned char*)(&viewProjection),
		(unsigned char*)(&viewProjection) + 128
	);
	MaterialManager::GlobalResources& resources =
		m_materialManager->getGlobalResources();
	resources.camera.transformation = m_camera.getViewVector();
	resources.camera.projection =
		glm::perspective(glm::radians(60.f), 800.f / 600.f, 0.1f, 1000.0f);

	MaterialManager::DescriptorSet globalDescriptor =
		m_materialManager->getGlobalDescriptorSet();

	for (auto drawable : m_currentScene->getDrawables()) {
		auto materialInstance =
			m_materialManager->getMaterial(drawable.m_materialInstance);

		commandBuffer.bindPipeline(
			vk::PipelineBindPoint::eGraphics,
			materialInstance.pipeline.m_pipeline
		);

		commandBuffer.pushConstants(
			materialInstance.pipeline.m_pipelineLayout,
			vk::ShaderStageFlagBits::eVertex,
			0,
			64,
			&drawable.m_modelMatrix
		);

		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			materialInstance.pipeline.m_pipelineLayout,
			0,
			{ globalDescriptor.set, materialInstance.instanceSet.set },
			nullptr
		);

		commandBuffer.bindVertexBuffers(
			0, { drawable.m_vertexBuffer.buffer }, { 0 }
		);
		commandBuffer.bindIndexBuffer(
			drawable.m_indexBuffer.buffer, 0, vk::IndexType::eUint32
		);

		commandBuffer.drawIndexed(drawable.m_indexCount, 1, 0, 0, 0);
	}

	commandBuffer.endRendering();

	currentImage.layoutTransition(
		commandBuffer, vk::ImageLayout::ePresentSrcKHR
	);
	currentImage.layout = vk::ImageLayout::ePresentSrcKHR;

	commandBuffer.end();

	vk::SubmitInfo submitInfo;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = { &frame.imageAvailable };
	auto stage = vk::Flags(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	submitInfo.pWaitDstStageMask = { &stage };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = { &commandBuffer };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = { &frame.renderFinished };

	m_graphicsQueue.submit({ submitInfo }, { frame.fence });

	vk::PresentInfoKHR presentInfo {};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.renderFinished;

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = { &(m_swapchain->getSwapchain()) };

	presentInfo.pImageIndices = &imageIndex;

	m_presentQueue.presentKHR(presentInfo);
};

void Renderer::load(const std::filesystem::path& path) {
	m_currentScene = std::make_unique<Scene>(
		Scene::loadMesh(path, *m_allocator, *m_materialManager)
	);
}