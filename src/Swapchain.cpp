#include "Swapchain.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Frame.hpp"
#include "resources/Image.hpp"

Swapchain::Swapchain(Instance& instance) :
	m_device(instance.device),
	m_physicalDevice(instance.physicalDevice),
	m_surface(instance.surface),
	m_surfaceFormat(instance.surfaceFormat),
	m_graphicsIndex(instance.queueFamiliesIndices.graphicsIndex) {
	createSwapchain();
	createFrames();
	createImages();
}
void Swapchain::createSwapchain() {
	vk::SurfaceCapabilitiesKHR capabilities =
		m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
	if (capabilities.currentExtent.width != UINT32_MAX)
		m_resolution = capabilities.currentExtent;

	vk::SwapchainCreateInfoKHR info;
	info.surface = m_surface;
	info.minImageCount = 3;
	info.imageFormat = m_surfaceFormat.format;
	info.imageExtent = m_resolution;
	info.imageArrayLayers = 1;
	info.imageUsage = vk::ImageUsageFlagBits::eTransferDst |
	                  vk::ImageUsageFlagBits::eColorAttachment;
	info.imageColorSpace = m_surfaceFormat.colorSpace;
	info.imageSharingMode = vk::SharingMode::eExclusive;

	info.queueFamilyIndexCount = 1;
	info.pQueueFamilyIndices = &m_graphicsIndex;
	info.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	info.clipped = vk::True;
	info.oldSwapchain = m_swapchain != nullptr ? m_swapchain : nullptr;

	m_swapchain = m_device.createSwapchainKHR(info);
}

void Swapchain::createFrames() {
	m_frames.reserve(3);

	vk::FenceCreateInfo fenceInfo { .flags =
		                                vk::FenceCreateFlagBits::eSignaled };

	vk::CommandPoolCreateInfo poolInfo;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
	poolInfo.queueFamilyIndex = m_graphicsIndex;
	for (int i = 0; i < 3; i++) {
		m_frames.push_back(Frame {
			.commandPool = m_device.createCommandPool(poolInfo),
			.fence = m_device.createFence(fenceInfo),
			.imageAvailable = m_device.createSemaphore({}),
			.renderFinished = m_device.createSemaphore({}),
		});
	}
}

void Swapchain::createImages() {
	std::vector<vk::Image> images = m_device.getSwapchainImagesKHR(m_swapchain);

	m_images.reserve(3);
	for (int i = 0; i < 3; i++) {
		vk::ImageViewCreateInfo viewInfo {
			.image = images[i],
			.viewType = vk::ImageViewType::e2D,
			.format = m_surfaceFormat.format,
			.subresourceRange = vk::ImageSubresourceRange(
				vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
			),
		};
		vk::ImageView view = m_device.createImageView(viewInfo);

		m_images.push_back(
			Image {
				.image = images[i],
				.view = view,
				.format = m_surfaceFormat.format,
				.size = {
					.width = m_resolution.width,
					.height = m_resolution.height,
					.depth = 1,
				},
				.allocation = std::nullopt,
				.accesses = {
					{
						.view = view,
						.layout = vk::ImageLayout::eUndefined,
					}
				},
				.transient= false,
			}
		);
	}
}
void Swapchain::rebuild() {
	m_images.clear();
	createSwapchain();
	createImages();
}
void Swapchain::flush() {
	if (!m_oldSwapchain.has_value()) return;
	m_device.destroySwapchainKHR(m_oldSwapchain.value());
	m_oldSwapchain = std::nullopt;
}