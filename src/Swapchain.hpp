#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Frame.hpp"
#include "resources/Image.hpp"

class Swapchain {
public:
	struct SwapchainCreateInfo;

private:
	static const short BUFFERING_COUNT = 2;
	std::vector<Frame> m_frames;
	std::vector<Image> m_images;

	int m_currentFrame = 0;
	vk::SwapchainKHR m_swapchain;

	void createFrames(
		vk::Device& device, vk::Format format, uint32_t familyIndex
	);

public:
	Swapchain(vk::Device& device, SwapchainCreateInfo& info);

	inline const Frame& getNextFrame() {
		m_currentFrame = (m_currentFrame + 1) % BUFFERING_COUNT;
		return m_frames[m_currentFrame];
	}
	inline const Frame& getPreviousFrame() {
		int frame = (m_currentFrame - 1 + BUFFERING_COUNT) % BUFFERING_COUNT;
		return m_frames[frame];
	};
	inline int getCurrentFrameIndex() { return m_currentFrame; }
	inline Image& getImage(uint32_t index) { return m_images.at(index); }
	inline const vk::SwapchainKHR& getSwapchain() const { return m_swapchain; };
};

struct Swapchain::SwapchainCreateInfo {
	int width;
	int height;
	uint32_t graphicsFamilyIndex;
	vk::SurfaceKHR& surface;
	vk::SurfaceFormatKHR format;
};