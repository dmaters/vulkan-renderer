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
	vk::Device& m_device;
	vk::PhysicalDevice& m_physicalDevice;
	vk::SurfaceKHR& m_surface;
	vk::SurfaceFormatKHR m_surfaceFormat;
	uint32_t m_graphicsIndex;

	vk::SwapchainKHR m_swapchain = nullptr;
	std::optional<vk::SwapchainKHR> m_oldSwapchain;

	vk::Extent2D m_resolution;

	std::vector<Frame> m_frames;
	std::vector<Image> m_images;

	void createFrames();
	void createImages();
	void createSwapchain();

public:
	Swapchain(Instance& instance);

	const Frame& getFrame(uint8_t index) { return m_frames[index]; }
	Image& getImage(uint8_t index) { return m_images.at(index); }
	const vk::SwapchainKHR& getSwapchain() const { return m_swapchain; };
	vk::Extent2D getResolution() { return m_resolution; }
	void rebuild();
	void flush();
};
