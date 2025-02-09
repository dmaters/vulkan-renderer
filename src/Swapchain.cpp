#include "Swapchain.hpp"

#include <cstdint>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Frame.hpp"
#include "resources/Image.hpp"

Swapchain::Swapchain(vk::Device& device, SwapchainCreateInfo& userInfo) {
	vk::SwapchainCreateInfoKHR info;
	info.surface = userInfo.surface;
	info.minImageCount = BUFFERING_COUNT;
	info.imageFormat = userInfo.format.format;
	info.imageExtent = vk::Extent2D(userInfo.width, userInfo.height);
	info.imageArrayLayers = 1;
	info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	info.imageColorSpace = userInfo.format.colorSpace;
	info.imageSharingMode = vk::SharingMode::eExclusive;

	info.queueFamilyIndexCount = 1;
	info.pQueueFamilyIndices = { &userInfo.graphicsFamilyIndex };
	info.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	info.clipped = vk::True;
	info.oldSwapchain = nullptr;

	m_swapchain = device.createSwapchainKHR(info);
	createFrames(device, userInfo.format.format, userInfo.graphicsFamilyIndex);
}

void Swapchain::createFrames(
	vk::Device& device, vk::Format format, uint32_t familyIndex
) {
	m_frames.reserve(BUFFERING_COUNT);
	m_images.reserve(BUFFERING_COUNT);

	vk::FenceCreateInfo fenceInfo { .flags =
		                                vk::FenceCreateFlagBits::eSignaled };
	std::vector<vk::Image> images = device.getSwapchainImagesKHR(m_swapchain);

	vk::CommandPoolCreateInfo poolInfo;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
	poolInfo.queueFamilyIndex = familyIndex;
	for (int i = 0; i < BUFFERING_COUNT; i++) {
		m_frames.push_back(std::move(Frame {

			.commandPool = device.createCommandPool(poolInfo),
			.fence = device.createFence(fenceInfo),
			.imageAvailable = device.createSemaphore({}),
			.renderFinished = device.createSemaphore({}),
		}));
		vk::ImageViewCreateInfo viewInfo {
			.image = images[i],
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = vk::ImageSubresourceRange(
				vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
			),
		};

		m_images.push_back(

			std::move(Image {
				.image = images[i],
				.view = device.createImageView(viewInfo),
				.allocation = std::nullopt,
				.format = format,
				.layout = vk::ImageLayout::eUndefined,

			})
		);
	}
}
