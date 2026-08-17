#include "Swapchain.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"

Swapchain Swapchain::Create() {
	Swapchain swapchain;

	Instance& instance = Instance::Get();

	vk::SurfaceCapabilitiesKHR capabilities = instance.physicalDevice.getSurfaceCapabilitiesKHR(instance.surface);
	if (capabilities.currentExtent.width != UINT32_MAX && capabilities.currentExtent != vk::Extent2D(0))
		swapchain.m_resolution = capabilities.currentExtent;
	swapchain.createSwapchain();
	swapchain.createFrames();
	swapchain.createImages();

	return swapchain;
}
void Swapchain::createSwapchain() {
	Instance& instance = Instance::Get();

	vk::SwapchainCreateInfoKHR info;
	info.surface = instance.surface;
	info.minImageCount = 3;
	info.imageFormat = instance.surfaceFormat.format;
	info.imageExtent = m_resolution;
	info.imageArrayLayers = 1;
	info.imageUsage = vk::ImageUsageFlagBits::eTransferDst |
	                  vk::ImageUsageFlagBits::eColorAttachment;
	info.imageColorSpace = instance.surfaceFormat.colorSpace;
	info.imageSharingMode = vk::SharingMode::eExclusive;
	info.presentMode = vk::PresentModeKHR::eFifo;
	info.queueFamilyIndexCount = 1;
	info.pQueueFamilyIndices = &instance.queueFamiliesIndices.graphicsIndex;
	info.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	info.clipped = vk::True;
	info.oldSwapchain = m_swapchain != nullptr ? m_swapchain : nullptr;

	m_swapchain = instance.device.createSwapchainKHR(info);
}

void Swapchain::createFrames() {
	Instance& instance = Instance::Get();

	vk::FenceCreateInfo fenceInfo { .flags =
		                                vk::FenceCreateFlagBits::eSignaled };

	vk::CommandPoolCreateInfo poolInfo;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
	poolInfo.queueFamilyIndex = instance.queueFamiliesIndices.graphicsIndex;
	for (int i = 0; i < 3; i++) {
		m_frames[i] = {
			.commandPool = instance.device.createCommandPool(poolInfo),
			.fence = instance.device.createFence(fenceInfo),
			.imageAvailable = instance.device.createSemaphore({}),
		};
	}
}

void Swapchain::createImages() {
	Instance& instance = Instance::Get();

	std::vector<vk::Image> images =
		instance.device.getSwapchainImagesKHR(m_swapchain);

	for (int i = 0; i < 3; i++) {
		vk::ImageViewCreateInfo viewInfo {
			.image = images[i],
			.viewType = vk::ImageViewType::e2D,
			.format = instance.surfaceFormat.format,
			.subresourceRange = vk::ImageSubresourceRange(
				vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
			),
		};
		vk::ImageView view = instance.device.createImageView(viewInfo);

		m_images[i] = {
			.image = images[i],
			.view = view,
			.format = instance.surfaceFormat.format,
			.size = {
				.width = m_resolution.width,
				.height = m_resolution.height,
				.depth = 1,
			},
			.allocation = std::nullopt,
		};
	}
}
void Swapchain::rebuild() {
	Instance& instance = Instance::Get();

	vk::SurfaceCapabilitiesKHR capabilities =
		instance.physicalDevice.getSurfaceCapabilitiesKHR(instance.surface);
	if (capabilities.currentExtent.width != UINT32_MAX)
		m_resolution = capabilities.currentExtent;

	if (m_resolution == vk::Extent2D(0)) return;

	createSwapchain();
	createImages();
}
void Swapchain::flush() {
	if (!m_oldSwapchain.has_value()) return;
	Instance::Get().device.destroySwapchainKHR(m_oldSwapchain.value());
	m_oldSwapchain = std::nullopt;
}
