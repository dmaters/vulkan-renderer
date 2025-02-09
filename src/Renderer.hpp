#pragma once
#include <vulkan/vulkan_hpp_macros.hpp>

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

#include <SDL3/SDL_video.h>
#include <VkBootstrap.h>

#include <filesystem>
#include <vulkan/vulkan.hpp>

#include "Camera.hpp"
#include "Instance.hpp"
#include "Scene.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "resources/MemoryAllocator.hpp"

class Renderer {
public:
private:
	Instance m_instance;
	vk::SurfaceKHR m_surface;
	vk::Queue m_graphicsQueue;
	vk::Queue m_presentQueue;
	vk::CommandPool m_commandPool;
	vk::DescriptorPool m_descriptorPool;

	std::unique_ptr<MemoryAllocator> m_allocator;
	std::unique_ptr<Swapchain> m_swapchain;
	std::unique_ptr<MaterialManager> m_materialManager;
	std::unique_ptr<Scene> m_currentScene;

	Camera m_camera;

	void createDevice(vkb::Instance& instance);
	void createSwapchain();

public:
	Renderer(SDL_Window* window);
	void load(const std::filesystem::path& path);
	void render();

	inline Camera& getCamera() { return m_camera; }
};