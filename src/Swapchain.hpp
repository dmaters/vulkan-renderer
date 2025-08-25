#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Frame.hpp"
#include "resources/Image.hpp"

class Swapchain {
private:
	vk::SwapchainKHR m_swapchain = nullptr;
	std::optional<vk::SwapchainKHR> m_oldSwapchain;

	vk::Extent2D m_resolution;

	std::array<Frame, 3> m_frames;
	std::array<Image, 3> m_images;

	void createFrames();
	void createImages();
	void createSwapchain();

public:
	Swapchain();

	const Frame& getFrame(uint8_t index) { return m_frames[index]; }
	Image& getImage(uint8_t index) { return m_images.at(index); }
	const vk::SwapchainKHR& getSwapchain() const { return m_swapchain; };
	vk::Extent2D getResolution() { return m_resolution; }
	void rebuild();
	void flush();
};
